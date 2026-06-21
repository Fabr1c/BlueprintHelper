#include "Systems/GraphLayout/BlueprintHelperGraphLayoutApplyScheduler.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/PlatformTime.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperGraphLayoutApplyScheduler, Log, All);

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPendingApply
{
	TWeakObjectPtr<UEdGraph> Graph;
	FLayoutPlan Plan;
	FRuleSet RuleSet;
	int32 NextPlacementIndex = 0;
	bool bChangedGraph = false;
};

struct FGraphLayoutApplySchedulerPrivate
{
	static TArray<FGraphLayoutPendingApply> PendingApplies;
	static FTSTicker::FDelegateHandle TickerHandle;

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

	static UEdGraphNode* FindNodeByLayoutId(UEdGraph* Graph, const FString& NodeId)
	{
		if (!Graph || NodeId.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && (MakeNodeId(Node) == NodeId || Node->GetName() == NodeId))
			{
				return Node;
			}
		}
		return nullptr;
	}

	static bool ApplyOnePlacement(
		UEdGraph* Graph,
		const FNodePlacement& Placement)
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
		const FRuleSet& RuleSet)
	{
		if (!Graph || !bChangedGraph)
		{
			return;
		}

		Graph->NotifyGraphChanged();
		UPackage* Package = Graph->GetOutermost();
		if ((RuleSet.bMarkDirtyAfterApply || RuleSet.bSaveAfterApply) && Package)
		{
			Package->MarkPackageDirty();
		}
		if (RuleSet.bSaveAfterApply && Package)
		{
			UE_LOG(LogBlueprintHelperGraphLayoutApplyScheduler, Verbose,
				TEXT("GraphLayout save_after_apply requested for %s; task flush defers saving to TaskRuntime post-operations."),
				*Package->GetName());
		}
	}

	static bool HasTimeBudgetRemaining(
		const double StartedAtSeconds,
		const int32 ProcessedPlacements,
		const FRuleSet& RuleSet)
	{
		if (ProcessedPlacements <= 0)
		{
			return true;
		}

		const float MaxMilliseconds = FMath::Max(0.0f, RuleSet.MaxMillisecondsPerFrame);
		if (MaxMilliseconds <= 0.0f)
		{
			return false;
		}
		return ((FPlatformTime::Seconds() - StartedAtSeconds) * 1000.0) < MaxMilliseconds;
	}

	static void EnsureTickerStarted()
	{
		if (TickerHandle.IsValid())
		{
			return;
		}

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&FGraphLayoutApplyScheduler::Tick));
	}
};

TArray<FGraphLayoutPendingApply> FGraphLayoutApplySchedulerPrivate::PendingApplies;
FTSTicker::FDelegateHandle FGraphLayoutApplySchedulerPrivate::TickerHandle;

void FGraphLayoutApplyScheduler::Enqueue(
	TWeakObjectPtr<UEdGraph> Graph,
	FLayoutPlan Plan,
	FRuleSet RuleSet)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [Graph, Plan = MoveTemp(Plan), RuleSet]()
		{
			FGraphLayoutApplyScheduler::Enqueue(Graph, Plan, RuleSet);
		});
		return;
	}

	if (!Graph.IsValid())
	{
		return;
	}

	FGraphLayoutPendingApply& Pending = FGraphLayoutApplySchedulerPrivate::PendingApplies.AddDefaulted_GetRef();
	Pending.Graph = Graph;
	Pending.Plan = MoveTemp(Plan);
	Pending.RuleSet = RuleSet;
	FGraphLayoutApplySchedulerPrivate::EnsureTickerStarted();
}

bool FGraphLayoutApplyScheduler::Tick(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (!IsInGameThread())
	{
		return true;
	}

	int32 ProcessedPlacements = 0;
	const double StartedAtSeconds = FPlatformTime::Seconds();

	while (FGraphLayoutApplySchedulerPrivate::PendingApplies.Num() > 0)
	{
		FGraphLayoutPendingApply& Pending = FGraphLayoutApplySchedulerPrivate::PendingApplies[0];
		UEdGraph* Graph = Pending.Graph.Get();
		if (!Graph)
		{
			FGraphLayoutApplySchedulerPrivate::PendingApplies.RemoveAt(0);
			continue;
		}

		const int32 MaxNodesPerFrame = FMath::Max(1, Pending.RuleSet.MaxNodesPerFrame);
		while (Pending.NextPlacementIndex < Pending.Plan.Placements.Num())
		{
			if (ProcessedPlacements >= MaxNodesPerFrame ||
				!FGraphLayoutApplySchedulerPrivate::HasTimeBudgetRemaining(
					StartedAtSeconds,
					ProcessedPlacements,
					Pending.RuleSet))
			{
				return true;
			}

			const FNodePlacement& Placement = Pending.Plan.Placements[Pending.NextPlacementIndex++];
			Pending.bChangedGraph =
				FGraphLayoutApplySchedulerPrivate::ApplyOnePlacement(Graph, Placement) || Pending.bChangedGraph;
			++ProcessedPlacements;
		}

		FGraphLayoutApplySchedulerPrivate::FinishChangedGraph(Graph, Pending.bChangedGraph, Pending.RuleSet);
		FGraphLayoutApplySchedulerPrivate::PendingApplies.RemoveAt(0);
	}

	if (FGraphLayoutApplySchedulerPrivate::TickerHandle.IsValid())
	{
		FGraphLayoutApplySchedulerPrivate::TickerHandle.Reset();
	}
	return false;
}

void FGraphLayoutApplyScheduler::ResetForTests()
{
	FGraphLayoutApplySchedulerPrivate::PendingApplies.Reset();
	if (FGraphLayoutApplySchedulerPrivate::TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FGraphLayoutApplySchedulerPrivate::TickerHandle);
		FGraphLayoutApplySchedulerPrivate::TickerHandle.Reset();
	}
}

int32 FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests()
{
	int32 Count = 0;
	for (const FGraphLayoutPendingApply& Pending : FGraphLayoutApplySchedulerPrivate::PendingApplies)
	{
		Count += FMath::Max(0, Pending.Plan.Placements.Num() - Pending.NextPlacementIndex);
	}
	return Count;
}
}
