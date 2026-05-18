#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSnapshotBuilder.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperGraphLayout, Log, All);

namespace BlueprintHelperGraphLayoutCoordinatorLocal
{
struct FPendingGraphLayout
{
	TWeakObjectPtr<UEdGraph> Graph;
	TSet<FString> GeneratedNodeIds;
};

struct FQueuedApply
{
	TWeakObjectPtr<UEdGraph> Graph;
	BlueprintHelper::GraphLayout::FLayoutPlan Plan;
	BlueprintHelper::GraphLayout::FRuleSet RuleSet;
	int32 NextPlacementIndex = 0;
	bool bChangedGraph = false;
};

static TArray<FPendingGraphLayout> GPendingLayouts;
static TArray<TSharedPtr<FQueuedApply>> GApplyQueue;
static FTSTicker::FDelegateHandle GApplyTickerHandle;
static bool bShuttingDown = false;

static FString MakeNodeId(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	return Node->NodeGuid.IsValid()
		? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens)
		: Node->GetName();
}

static FString GetRuleSetConfigPath()
{
	return FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath();
}

static BlueprintHelper::GraphLayout::FRuleSet LoadConfiguredRuleSet()
{
	BlueprintHelper::GraphLayout::FRuleSet RuleSet;
	const FString ConfigPath = GetRuleSetConfigPath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		return RuleSet;
	}

	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(JsonText, RuleSet, Validation))
	{
		UE_LOG(LogBlueprintHelperGraphLayout, Warning, TEXT("GraphLayout RuleSet import failed; using defaults. %s"),
			*FString::Join(Validation.Errors, TEXT(" ")));
		return BlueprintHelper::GraphLayout::FRuleSet();
	}
	return RuleSet;
}

static FPendingGraphLayout* FindPendingLayout(UEdGraph* Graph)
{
	for (FPendingGraphLayout& Pending : GPendingLayouts)
	{
		if (Pending.Graph.Get() == Graph)
		{
			return &Pending;
		}
	}
	return nullptr;
}

static UEdGraphNode* FindNodeByLayoutId(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph || NodeId.IsEmpty())
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (MakeNodeId(Node) == NodeId || Node->GetName() == NodeId)
		{
			return Node;
		}
	}
	return nullptr;
}

static bool ApplyOnePlacement(UEdGraph* Graph, const BlueprintHelper::GraphLayout::FNodePlacement& Placement)
{
	if (!Graph || !Placement.bMoveExisting)
	{
		return false;
	}

	UEdGraphNode* Node = FindNodeByLayoutId(Graph, Placement.NodeId);
	if (!Node)
	{
		return false;
	}

	const int32 TargetX = FMath::RoundToInt(Placement.TargetPosition.X);
	const int32 TargetY = FMath::RoundToInt(Placement.TargetPosition.Y);
	if (Node->NodePosX == TargetX && Node->NodePosY == TargetY)
	{
		return false;
	}

	Node->Modify();
	Node->NodePosX = TargetX;
	Node->NodePosY = TargetY;
	return true;
}

static void FinishApplyJob(const TSharedPtr<FQueuedApply>& ApplyJob)
{
	if (!ApplyJob.IsValid())
	{
		return;
	}

	UEdGraph* Graph = ApplyJob->Graph.Get();
	if (Graph && ApplyJob->bChangedGraph)
	{
		Graph->NotifyGraphChanged();
		UPackage* Package = Graph->GetOutermost();
		if ((ApplyJob->RuleSet.bMarkDirtyAfterApply || ApplyJob->RuleSet.bSaveAfterApply) && Package)
		{
			Package->MarkPackageDirty();
		}
		if (ApplyJob->RuleSet.bSaveAfterApply && Package)
		{
			const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages({Package}, true);
			if (!bSaved)
			{
				UE_LOG(LogBlueprintHelperGraphLayout, Warning, TEXT("GraphLayout save_after_apply failed for package %s."), *Package->GetName());
			}
		}
	}
}

static bool TickApplyQueue(float)
{
	if (bShuttingDown)
	{
		GApplyQueue.Reset();
		GApplyTickerHandle.Reset();
		return false;
	}

	const double FrameStartSeconds = FPlatformTime::Seconds();
	for (int32 QueueIndex = 0; QueueIndex < GApplyQueue.Num();)
	{
		TSharedPtr<FQueuedApply> ApplyJob = GApplyQueue[QueueIndex];
		if (!ApplyJob.IsValid() || !ApplyJob->Graph.IsValid())
		{
			GApplyQueue.RemoveAt(QueueIndex);
			continue;
		}

		UEdGraph* Graph = ApplyJob->Graph.Get();
		const int32 MaxNodesThisFrame = FMath::Max(1, ApplyJob->RuleSet.MaxNodesPerFrame);
		const double MaxSecondsThisFrame = FMath::Max(0.25f, ApplyJob->RuleSet.MaxMillisecondsPerFrame) / 1000.0;
		int32 MovedThisFrame = 0;

		while (ApplyJob->NextPlacementIndex < ApplyJob->Plan.Placements.Num())
		{
			const BlueprintHelper::GraphLayout::FNodePlacement& Placement =
				ApplyJob->Plan.Placements[ApplyJob->NextPlacementIndex++];
			if (ApplyOnePlacement(Graph, Placement))
			{
				ApplyJob->bChangedGraph = true;
				++MovedThisFrame;
			}

			const double ElapsedSeconds = FPlatformTime::Seconds() - FrameStartSeconds;
			if (MovedThisFrame >= MaxNodesThisFrame || ElapsedSeconds >= MaxSecondsThisFrame)
			{
				break;
			}
		}

		if (ApplyJob->NextPlacementIndex >= ApplyJob->Plan.Placements.Num())
		{
			FinishApplyJob(ApplyJob);
			GApplyQueue.RemoveAt(QueueIndex);
			continue;
		}

		++QueueIndex;
		const double ElapsedSeconds = FPlatformTime::Seconds() - FrameStartSeconds;
		if (ElapsedSeconds >= MaxSecondsThisFrame)
		{
			break;
		}
	}

	if (GApplyQueue.Num() == 0)
	{
		GApplyTickerHandle.Reset();
		return false;
	}
	return true;
}

static void EnsureApplyTicker()
{
	if (!GApplyTickerHandle.IsValid())
	{
		GApplyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&TickApplyQueue));
	}
}

static void EnqueueApply(
	TWeakObjectPtr<UEdGraph> Graph,
	const BlueprintHelper::GraphLayout::FLayoutPlan& Plan,
	const BlueprintHelper::GraphLayout::FRuleSet& RuleSet)
{
	if (bShuttingDown || !Graph.IsValid())
	{
		return;
	}

	TSharedPtr<FQueuedApply> ApplyJob = MakeShared<FQueuedApply>();
	ApplyJob->Graph = Graph;
	ApplyJob->Plan = Plan;
	ApplyJob->RuleSet = RuleSet;
	GApplyQueue.Add(ApplyJob);
	EnsureApplyTicker();
}

static void StartSnapshotAndSolve(const FPendingGraphLayout& Pending)
{
	TWeakObjectPtr<UEdGraph> Graph = Pending.Graph;
	const TSet<FString> GeneratedNodeIds = Pending.GeneratedNodeIds;

	AsyncTask(ENamedThreads::GameThread, [Graph, GeneratedNodeIds]()
	{
		if (bShuttingDown || !Graph.IsValid())
		{
			return;
		}

		BlueprintHelper::GraphLayout::FGraphSnapshot Snapshot =
			BlueprintHelper::GraphLayout::FSnapshotBuilder::CaptureGraph(Graph.Get());
		for (BlueprintHelper::GraphLayout::FNodeSnapshot& Node : Snapshot.Nodes)
		{
			Node.bExisting = !GeneratedNodeIds.Contains(Node.NodeId);
		}

		BlueprintHelper::GraphLayout::FRuleSet RuleSet = LoadConfiguredRuleSet();
		Async(EAsyncExecution::ThreadPool, [Graph, Snapshot = MoveTemp(Snapshot), RuleSet]()
		{
			const BlueprintHelper::GraphLayout::FLayoutPlan Plan =
				BlueprintHelper::GraphLayout::FSolver::Solve(Snapshot, RuleSet);
			AsyncTask(ENamedThreads::GameThread, [Graph, Plan, RuleSet]()
			{
				EnqueueApply(Graph, Plan, RuleSet);
			});
		});
	});
}
}

void FBlueprintHelperGraphLayoutCoordinator::Startup()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	bShuttingDown = false;
	GPendingLayouts.Reset();
	GApplyQueue.Reset();
	if (GApplyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GApplyTickerHandle);
		GApplyTickerHandle.Reset();
	}
}

void FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(
	UEdGraph* Graph,
	const TArray<UEdGraphNode*>& GeneratedNodes)
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	if (!Graph || GeneratedNodes.Num() == 0)
	{
		return;
	}

	if (!IsInGameThread())
	{
		TWeakObjectPtr<UEdGraph> GraphWeak(Graph);
		TArray<TWeakObjectPtr<UEdGraphNode>> NodeWeakRefs;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			NodeWeakRefs.Add(Node);
		}
		AsyncTask(ENamedThreads::GameThread, [GraphWeak, NodeWeakRefs]()
		{
			TArray<UEdGraphNode*> Nodes;
			for (const TWeakObjectPtr<UEdGraphNode>& NodeWeak : NodeWeakRefs)
			{
				if (NodeWeak.IsValid())
				{
					Nodes.Add(NodeWeak.Get());
				}
			}
			FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(GraphWeak.Get(), Nodes);
		});
		return;
	}

	if (bShuttingDown)
	{
		return;
	}

	FPendingGraphLayout* Pending = FindPendingLayout(Graph);
	if (!Pending)
	{
		FPendingGraphLayout NewPending;
		NewPending.Graph = Graph;
		GPendingLayouts.Add(NewPending);
		Pending = &GPendingLayouts.Last();
	}

	for (UEdGraphNode* Node : GeneratedNodes)
	{
		if (Node)
		{
			Pending->GeneratedNodeIds.Add(MakeNodeId(Node));
		}
	}
}

void FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	if (bShuttingDown || GPendingLayouts.Num() == 0)
	{
		return;
	}

	TArray<FPendingGraphLayout> PendingLayouts = MoveTemp(GPendingLayouts);
	GPendingLayouts.Reset();
	for (const FPendingGraphLayout& Pending : PendingLayouts)
	{
		if (Pending.Graph.IsValid() && Pending.GeneratedNodeIds.Num() > 0)
		{
			StartSnapshotAndSolve(Pending);
		}
	}
}

void FBlueprintHelperGraphLayoutCoordinator::DiscardPendingTaskLayouts()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	GPendingLayouts.Reset();
}

void FBlueprintHelperGraphLayoutCoordinator::Shutdown()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	bShuttingDown = true;
	GPendingLayouts.Reset();
	GApplyQueue.Reset();
	if (GApplyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GApplyTickerHandle);
		GApplyTickerHandle.Reset();
	}
}

FString FBlueprintHelperGraphLayoutCoordinator::GetDefaultRuleSetJson()
{
	return BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(
		BlueprintHelper::GraphLayout::FRuleSet());
}

FString FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	const FString ConfigPath = GetRuleSetConfigPath();
	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		return JsonText;
	}
	return GetDefaultRuleSetJson();
}

bool FBlueprintHelperGraphLayoutCoordinator::SaveConfiguredRuleSetJson(const FString& JsonText)
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	FString ValidationMessage;
	if (!ValidateRuleSetJson(JsonText, ValidationMessage))
	{
		UE_LOG(LogBlueprintHelperGraphLayout, Warning, TEXT("GraphLayout RuleSet export rejected: %s"), *ValidationMessage);
		return false;
	}

	const FString ConfigPath = GetRuleSetConfigPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ConfigPath), true);
	return FFileHelper::SaveStringToFile(JsonText, *ConfigPath);
}

bool FBlueprintHelperGraphLayoutCoordinator::ValidateRuleSetJson(
	const FString& JsonText,
	FString& OutMessage)
{
	BlueprintHelper::GraphLayout::FRuleSet RuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(JsonText, RuleSet, Validation))
	{
		OutMessage = Validation.Errors.Num() > 0
			? FString::Join(Validation.Errors, TEXT(" "))
			: TEXT("RuleSet JSON is invalid.");
		return false;
	}

	OutMessage = Validation.Warnings.Num() > 0
		? FString::Printf(TEXT("RuleSet JSON is valid with warnings: %s"), *FString::Join(Validation.Warnings, TEXT(" ")))
		: TEXT("RuleSet JSON is valid.");
	return true;
}
