#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "FileHelpers.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSourceResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSnapshotBuilder.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "Templates/Atomic.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperGraphLayout, Log, All);

namespace BlueprintHelperGraphLayoutCoordinatorLocal
{
struct FPendingGraphLayout
{
	TWeakObjectPtr<UEdGraph> Graph;
	TSet<FString> GeneratedNodeIds;
};

static TArray<FPendingGraphLayout> GPendingLayouts;
static bool bShuttingDown = false;
static constexpr uint32 SyncFlushWaitTimeoutMs = 30000;

struct FFlushCompletionSignal
{
	FFlushCompletionSignal()
		: Event(FPlatformProcess::GetSynchEventFromPool(true))
	{
	}

	~FFlushCompletionSignal()
	{
		if (Event)
		{
			FPlatformProcess::ReturnSynchEventToPool(Event);
			Event = nullptr;
		}
	}

	void Trigger() const
	{
		if (Event)
		{
			Event->Trigger();
		}
	}

	bool Wait(const uint32 TimeoutMs) const
	{
		return Event ? Event->Wait(TimeoutMs) : true;
	}

private:
	FEvent* Event = nullptr;
};

struct FGameThreadOperationState
{
	FGameThreadOperationState()
		: bSuccess(false)
		, bCancelled(false)
	{
	}

	FFlushCompletionSignal Completion;
	TAtomic<bool> bSuccess;
	TAtomic<bool> bCancelled;
};

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
	return FBlueprintHelperGraphLayoutRuleSourceResolver::ResolveRuleSourcePath();
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

static void FinishChangedGraph(
	UEdGraph* Graph,
	const bool bChangedGraph,
	const BlueprintHelper::GraphLayout::FRuleSet& RuleSet)
{
	if (Graph && bChangedGraph)
	{
		Graph->NotifyGraphChanged();
		UPackage* Package = Graph->GetOutermost();
		if ((RuleSet.bMarkDirtyAfterApply || RuleSet.bSaveAfterApply) && Package)
		{
			Package->MarkPackageDirty();
		}
		if (RuleSet.bSaveAfterApply && Package)
		{
			UE_LOG(LogBlueprintHelperGraphLayout, Verbose,
				TEXT("GraphLayout save_after_apply requested for %s; task flush defers saving to TaskRuntime post-operations."),
				*Package->GetName());
		}
	}
}

static void ApplyPlanImmediately(
	UEdGraph* Graph,
	const BlueprintHelper::GraphLayout::FLayoutPlan& Plan,
	const BlueprintHelper::GraphLayout::FRuleSet& RuleSet)
{
	bool bChangedGraph = false;
	for (const BlueprintHelper::GraphLayout::FNodePlacement& Placement : Plan.Placements)
	{
		bChangedGraph = ApplyOnePlacement(Graph, Placement) || bChangedGraph;
	}
	FinishChangedGraph(Graph, bChangedGraph, RuleSet);
}

static void SnapshotSolveAndApplyNow(const FPendingGraphLayout& Pending)
{
	if (bShuttingDown || !Pending.Graph.IsValid())
	{
		return;
	}

	UEdGraph* Graph = Pending.Graph.Get();
	BlueprintHelper::GraphLayout::FGraphSnapshot Snapshot =
		BlueprintHelper::GraphLayout::FSnapshotBuilder::CaptureGraph(Graph);
	for (BlueprintHelper::GraphLayout::FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Node.bExisting = !Pending.GeneratedNodeIds.Contains(Node.NodeId);
	}

	BlueprintHelper::GraphLayout::FRuleSet RuleSet = LoadConfiguredRuleSet();
	const BlueprintHelper::GraphLayout::FLayoutPlan Plan =
		BlueprintHelper::GraphLayout::FSolver::Solve(Snapshot, RuleSet);
	ApplyPlanImmediately(Graph, Plan, RuleSet);
}

static void SnapshotSolveAndApplyNow(TArray<FPendingGraphLayout>& PendingLayouts)
{
	for (const FPendingGraphLayout& Pending : PendingLayouts)
	{
		if (Pending.Graph.IsValid() && Pending.GeneratedNodeIds.Num() > 0)
		{
			SnapshotSolveAndApplyNow(Pending);
		}
	}
}

static bool FlushPendingTaskLayoutsOnGameThread()
{
	if (!IsInGameThread())
	{
		return false;
	}

	if (bShuttingDown)
	{
		GPendingLayouts.Reset();
		return true;
	}

	if (GPendingLayouts.Num() == 0)
	{
		return true;
	}

	TArray<FPendingGraphLayout> PendingLayouts = MoveTemp(GPendingLayouts);
	GPendingLayouts.Reset();
	SnapshotSolveAndApplyNow(PendingLayouts);
	return true;
}
}

void FBlueprintHelperGraphLayoutCoordinator::Startup()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	bShuttingDown = false;
	GPendingLayouts.Reset();
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
		TSharedRef<FGameThreadOperationState, ESPMode::ThreadSafe> OperationState =
			MakeShared<FGameThreadOperationState, ESPMode::ThreadSafe>();
		AsyncTask(ENamedThreads::GameThread, [GraphWeak, NodeWeakRefs, OperationState]()
		{
			if (OperationState->bCancelled.Load())
			{
				OperationState->Completion.Trigger();
				return;
			}

			TArray<UEdGraphNode*> Nodes;
			for (const TWeakObjectPtr<UEdGraphNode>& NodeWeak : NodeWeakRefs)
			{
				if (NodeWeak.IsValid())
				{
					Nodes.Add(NodeWeak.Get());
				}
			}
			FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(GraphWeak.Get(), Nodes);
			OperationState->bSuccess.Store(true);
			OperationState->Completion.Trigger();
		});
		if (!OperationState->Completion.Wait(SyncFlushWaitTimeoutMs))
		{
			OperationState->bCancelled.Store(true);
			UE_LOG(LogBlueprintHelperGraphLayout, Warning,
				TEXT("GraphLayout generated-node registration timed out after %u ms while waiting for the game thread."),
				SyncFlushWaitTimeoutMs);
		}
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

bool FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	if (IsInGameThread())
	{
		return FlushPendingTaskLayoutsOnGameThread();
	}

	TSharedRef<FGameThreadOperationState, ESPMode::ThreadSafe> OperationState =
		MakeShared<FGameThreadOperationState, ESPMode::ThreadSafe>();
	AsyncTask(ENamedThreads::GameThread, [OperationState]()
	{
		if (OperationState->bCancelled.Load())
		{
			OperationState->Completion.Trigger();
			return;
		}

		OperationState->bSuccess.Store(FlushPendingTaskLayoutsOnGameThread());
		OperationState->Completion.Trigger();
	});
	if (!OperationState->Completion.Wait(SyncFlushWaitTimeoutMs))
	{
		OperationState->bCancelled.Store(true);
		UE_LOG(LogBlueprintHelperGraphLayout, Warning,
			TEXT("GraphLayout synchronous flush timed out after %u ms while waiting for the game thread."),
			SyncFlushWaitTimeoutMs);
		return false;
	}
	return OperationState->bSuccess.Load();
}

void FBlueprintHelperGraphLayoutCoordinator::DiscardPendingTaskLayouts()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	if (IsInGameThread())
	{
		GPendingLayouts.Reset();
		return;
	}

	TSharedRef<FGameThreadOperationState, ESPMode::ThreadSafe> OperationState =
		MakeShared<FGameThreadOperationState, ESPMode::ThreadSafe>();
	AsyncTask(ENamedThreads::GameThread, [OperationState]()
	{
		if (!OperationState->bCancelled.Load())
		{
			GPendingLayouts.Reset();
			OperationState->bSuccess.Store(true);
		}
		OperationState->Completion.Trigger();
	});
	if (!OperationState->Completion.Wait(SyncFlushWaitTimeoutMs))
	{
		OperationState->bCancelled.Store(true);
		UE_LOG(LogBlueprintHelperGraphLayout, Warning,
			TEXT("GraphLayout discard timed out after %u ms while waiting for the game thread."),
			SyncFlushWaitTimeoutMs);
	}
}

void FBlueprintHelperGraphLayoutCoordinator::Shutdown()
{
	using namespace BlueprintHelperGraphLayoutCoordinatorLocal;

	bShuttingDown = true;
	GPendingLayouts.Reset();
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
