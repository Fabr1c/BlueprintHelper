#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

namespace BlueprintHelper::GraphLayout
{
struct FWorkingNode
{
	const FNodeSnapshot* Snapshot = nullptr;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Target = FVector2D::ZeroVector;
	bool bHasTarget = false;
	bool bPinnedToCurrentPosition = false;
	int32 SemanticRow = INDEX_NONE;
	FString Reason;
};

static bool IsExecRole(ENodeRole Role)
{
	return Role == ENodeRole::EventEntry ||
		Role == ENodeRole::ExecNode ||
		Role == ENodeRole::BranchControl ||
		Role == ENodeRole::AsyncNode ||
		Role == ENodeRole::DelegateNode;
}

static int32 ReserveRow(TMap<int32, TSet<int32>>& UsedRows, int32 Column, int32 PreferredRow)
{
	TSet<int32>& ColumnRows = UsedRows.FindOrAdd(Column);
	int32 Row = PreferredRow;
	while (ColumnRows.Contains(Row))
	{
		++Row;
	}
	ColumnRows.Add(Row);
	return Row;
}

static int32 GetRowForTargetY(const FVector2D& Target, const FRuleSet& RuleSet)
{
	const float RowSpacing = FMath::Max(1.0f, RuleSet.ExecRowSpacing);
	return FMath::CeilToInt(Target.Y / RowSpacing);
}

static void SetTarget(
	TMap<FString, FWorkingNode>& Nodes,
	FOccupancyResolver& Occupancy,
	const FString& NodeId,
	const FVector2D& DesiredTarget,
	const FString& Reason)
{
	if (FWorkingNode* Node = Nodes.Find(NodeId))
	{
		if (Node->bPinnedToCurrentPosition)
		{
			if (Node->Snapshot && !Node->bHasTarget)
			{
				Node->Target = Node->Snapshot->Position;
				Node->bHasTarget = true;
				Node->Reason = TEXT("existing_node_static_anchor");
			}
			return;
		}

		const FVector2D Size = Node->Snapshot ? Node->Snapshot->Size : FVector2D(180.0f, 80.0f);
		const FVector2D Target = Occupancy.ResolveNearestFreeTarget(NodeId, DesiredTarget, Size);
		Node->Target = Target;
		Node->bHasTarget = true;
		Node->Reason = Target.Equals(DesiredTarget)
			? Reason
			: FString::Printf(TEXT("%s_avoided_overlap"), *Reason);
		Occupancy.ReserveTarget(NodeId, Target, Size, true);
	}
}

static void ReserveImmovableExistingNodes(
	const FGraphSnapshot& Snapshot,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy)
{
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (Node.bExisting && !RuleSet.bMoveExistingNodes)
		{
			Occupancy.ReserveExistingNode(Node);
		}
	}
}

static void LayoutExecChain(
	const FString& RootNodeId,
	const FGraphTopology& Topology,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy,
	TMap<int32, TSet<int32>>& UsedRows,
	int32 RootRow,
	TSet<FString>& Visited)
{
	struct FQueueItem
	{
		FString NodeId;
		int32 Column = 0;
		int32 Row = 0;
	};

	TArray<FQueueItem> Queue;
	Queue.Add({ RootNodeId, 0, RootRow });

	for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
	{
		const FQueueItem Item = Queue[QueueIndex];
		if (Visited.Contains(Item.NodeId))
		{
			continue;
		}
		FWorkingNode* Node = Nodes.Find(Item.NodeId);
		if (!Node || !Node->Snapshot || !IsExecRole(Node->Role))
		{
			continue;
		}

		Visited.Add(Item.NodeId);
		const int32 Row = ReserveRow(UsedRows, Item.Column, Item.Row);
		const FVector2D Target(Item.Column * RuleSet.ExecColumnSpacing, Row * RuleSet.ExecRowSpacing);
		SetTarget(Nodes, Occupancy, Item.NodeId, Target, TEXT("exec_flow"));
		const int32 ResolvedRow = GetRowForTargetY(Node->Target, RuleSet);
		Node->SemanticRow = ResolvedRow;

		const bool bBranchLike = RuleSet.bAlignExecNodesHorizontally
			? Topology.IsMultiExecOutputNode(Item.NodeId)
			: Node->Role == ENodeRole::BranchControl;
		const float RowStep = bBranchLike
			? RuleSet.BranchRowSpacing / FMath::Max(1.0f, RuleSet.ExecRowSpacing)
			: 0.0f;

		const TArray<FExecEdge> Successors = Topology.GetExecOutputEdges(Item.NodeId);
		for (const FExecEdge& SuccessorEdge : Successors)
		{
			const FWorkingNode* Successor = Nodes.Find(SuccessorEdge.TargetNodeId);
			if (!Successor || !IsExecRole(Successor->Role))
			{
				continue;
			}

			const int32 BranchOffset = bBranchLike ? SuccessorEdge.SourceOutputOrdinal : 0;
			Queue.Add({
				SuccessorEdge.TargetNodeId,
				Item.Column + 1,
				ResolvedRow + FMath::RoundToInt(BranchOffset * RowStep)
			});
		}
	}
}

static bool AlignInputsToConsumerPinOrder(
	const FGraphSnapshot& Snapshot,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy)
{
	bool bChanged = false;
	for (const FNodeSnapshot& ConsumerSnapshot : Snapshot.Nodes)
	{
		FWorkingNode* ConsumerNode = Nodes.Find(ConsumerSnapshot.NodeId);
		if (!ConsumerNode || !ConsumerNode->Snapshot || !ConsumerNode->bHasTarget)
		{
			continue;
		}

		int32 InputOrder = 0;
		for (const FPinSnapshot& Pin : ConsumerNode->Snapshot->Pins)
		{
			if (Pin.Direction != EPinDirection::Input || Pin.bExec)
			{
				continue;
			}

			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				FWorkingNode* SourceNode = Nodes.Find(LinkedNodeId);
				if (!SourceNode || !SourceNode->Snapshot)
				{
					continue;
				}

				if (FDataInputPlacement::IsDataInputRole(SourceNode->Role) && !SourceNode->bHasTarget)
				{
					const int32 PlacementOrder =
						SourceNode->Role == ENodeRole::VariableInput && !RuleSet.bUseTargetPinOrderForVariableInputs
							? 0
							: InputOrder;

					FDataInputPlacementRequest Request;
					Request.ConsumerNodeId = ConsumerSnapshot.NodeId;
					Request.SourceNodeId = LinkedNodeId;
					Request.SourceRole = SourceNode->Role;
					Request.InputOrder = PlacementOrder;
					Request.ConsumerTarget = ConsumerNode->Target;
					Request.SourceSize = SourceNode->Snapshot->Size;

					const FVector2D DesiredTarget = FDataInputPlacement::BuildDesiredTarget(RuleSet, Request);
					SetTarget(
						Nodes,
						Occupancy,
						LinkedNodeId,
						DesiredTarget,
						FDataInputPlacement::GetReason(SourceNode->Role));
					bChanged = true;
				}
			}
			++InputOrder;
		}
	}
	return bChanged;
}

static TMap<int32, float> BuildAllocatedRowBaselines(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet)
{
	TMap<int32, float> MinHeightByRow;
	int32 MaxRow = INDEX_NONE;

	for (const TPair<FString, FWorkingNode>& Pair : Nodes)
	{
		const FWorkingNode& Node = Pair.Value;
		if (!Node.bHasTarget || Node.SemanticRow == INDEX_NONE || !IsExecRole(Node.Role))
		{
			continue;
		}

		MaxRow = FMath::Max(MaxRow, Node.SemanticRow);
		MinHeightByRow.FindOrAdd(Node.SemanticRow) = RuleSet.ExecRowSpacing;
	}

	if (MaxRow == INDEX_NONE)
	{
		return {};
	}

	for (const TPair<FString, FWorkingNode>& Pair : Nodes)
	{
		const FWorkingNode& Node = Pair.Value;
		if (!Node.bHasTarget || Node.SemanticRow == INDEX_NONE || !IsExecRole(Node.Role))
		{
			continue;
		}

		const FNodeInputClusterBudget ClusterBudget =
			FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, Pair.Key, RuleSet);
		float& RowMinHeight = MinHeightByRow.FindOrAdd(Node.SemanticRow);
		RowMinHeight = FMath::Max(RowMinHeight, ClusterBudget.Height);
	}

	TArray<FExecRowBudget> RowBudgets;
	RowBudgets.Reserve(MaxRow + 1);
	for (int32 Row = 0; Row <= MaxRow; ++Row)
	{
		FExecRowBudget& Budget = RowBudgets.AddDefaulted_GetRef();
		Budget.RowId = Row;
		Budget.MinHeight = MinHeightByRow.FindRef(Row);
	}

	TMap<int32, float> BaselinesByRow;
	for (const FExecRowAllocation& Allocation : FGraphLayoutRowAllocationPolicy::Allocate(RowBudgets, RuleSet))
	{
		BaselinesByRow.Add(Allocation.RowId, Allocation.BaselineY);
	}
	return BaselinesByRow;
}

static void ReflowExecTargetsToAllocatedRows(
	const FGraphSnapshot& Snapshot,
	const TMap<int32, float>& BaselinesByRow,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy)
{
	TArray<int32> OrderedRows;
	BaselinesByRow.GetKeys(OrderedRows);
	OrderedRows.Sort();

	TMap<int32, TArray<FString>> MovableExecNodeIdsByRow;
	for (const FNodeSnapshot& NodeSnapshot : Snapshot.Nodes)
	{
		FWorkingNode* Node = Nodes.Find(NodeSnapshot.NodeId);
		if (!Node || !Node->bHasTarget || !IsExecRole(Node->Role) || Node->SemanticRow == INDEX_NONE)
		{
			continue;
		}

		if (Node->bPinnedToCurrentPosition)
		{
			continue;
		}

		MovableExecNodeIdsByRow.FindOrAdd(Node->SemanticRow).Add(NodeSnapshot.NodeId);
	}

	float CumulativeBaselineBumpY = 0.0f;
	for (const int32 RowId : OrderedRows)
	{
		const float AllocatedBaselineY = BaselinesByRow.FindRef(RowId);
		const float DesiredBaselineY = AllocatedBaselineY + CumulativeBaselineBumpY;
		const TArray<FString>* RowNodeIds = MovableExecNodeIdsByRow.Find(RowId);
		if (!RowNodeIds || RowNodeIds->Num() == 0)
		{
			continue;
		}

		float ResolvedBaselineY = DesiredBaselineY;
		const int32 MaxBaselineRefinementPasses =
			FMath::Max(1, RuleSet.MaxCollisionAttempts * FMath::Max(1, RowNodeIds->Num()) + 1);
		for (int32 PassIndex = 0; PassIndex < MaxBaselineRefinementPasses; ++PassIndex)
		{
			float NextBaselineY = ResolvedBaselineY;
			for (const FString& NodeId : *RowNodeIds)
			{
				const FWorkingNode* Node = Nodes.Find(NodeId);
				if (!Node || !Node->Snapshot)
				{
					continue;
				}

				const FVector2D ResolvedTarget = Occupancy.ResolveNearestFreeTarget(
					NodeId,
					FVector2D(Node->Target.X, ResolvedBaselineY),
					Node->Snapshot->Size);
				NextBaselineY = FMath::Max(NextBaselineY, ResolvedTarget.Y);
			}

			if (FMath::IsNearlyEqual(NextBaselineY, ResolvedBaselineY))
			{
				break;
			}
			ResolvedBaselineY = NextBaselineY;
		}

		float FinalBaselineY = ResolvedBaselineY;
		for (const FString& NodeId : *RowNodeIds)
		{
			const FWorkingNode* Node = Nodes.Find(NodeId);
			if (!Node)
			{
				continue;
			}

			SetTarget(
				Nodes,
				Occupancy,
				NodeId,
				FVector2D(Node->Target.X, ResolvedBaselineY),
				TEXT("exec_flow"));

			if (FWorkingNode* UpdatedNode = Nodes.Find(NodeId))
			{
				FinalBaselineY = FMath::Max(FinalBaselineY, UpdatedNode->Target.Y);
				UpdatedNode->SemanticRow = GetRowForTargetY(UpdatedNode->Target, RuleSet);
			}
		}

		CumulativeBaselineBumpY = FMath::Max(CumulativeBaselineBumpY, FinalBaselineY - AllocatedBaselineY);
	}
}

static bool PlaceInputClusters(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy)
{
	bool bChanged = false;
	for (const FNodeSnapshot& ConsumerSnapshot : Snapshot.Nodes)
	{
		const FWorkingNode* ConsumerNode = Nodes.Find(ConsumerSnapshot.NodeId);
		if (!ConsumerNode || !ConsumerNode->bHasTarget)
		{
			continue;
		}

		const FNodeInputClusterBudget ClusterBudget =
			FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, ConsumerSnapshot.NodeId, RuleSet);
		for (const FString& ClusterNodeId : ClusterBudget.NodeIds)
		{
			FWorkingNode* ClusterNode = Nodes.Find(ClusterNodeId);
			if (!ClusterNode || !ClusterNode->Snapshot || ClusterNode->bHasTarget)
			{
				continue;
			}

			const FVector2D* RelativeTarget = ClusterBudget.RelativeTargets.Find(ClusterNodeId);
			if (!RelativeTarget)
			{
				continue;
			}

			SetTarget(
				Nodes,
				Occupancy,
				ClusterNodeId,
				ConsumerNode->Target + *RelativeTarget,
				TEXT("pure_data_subgraph_alignment"));
			bChanged = true;
		}
	}
	return bChanged;
}

FLayoutPlan FSolver::Solve(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet)
{
	FLayoutPlan Plan;
	Plan.Classifications = FClassifier::ClassifyGraph(Snapshot, RuleSet);
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);

	TMap<FString, ENodeRole> RolesById;
	for (const FNodeClassification& Classification : Plan.Classifications)
	{
		RolesById.Add(Classification.NodeId, Classification.Role);
	}

	TMap<FString, FWorkingNode> Nodes;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		FWorkingNode WorkingNode;
		WorkingNode.Snapshot = &Node;
		WorkingNode.Role = RolesById.FindRef(Node.NodeId);
		WorkingNode.bPinnedToCurrentPosition = Node.bExisting && !RuleSet.bMoveExistingNodes;
		if (WorkingNode.bPinnedToCurrentPosition)
		{
			WorkingNode.Target = Node.Position;
			WorkingNode.bHasTarget = true;
			WorkingNode.Reason = TEXT("existing_node_static_anchor");
		}
		Nodes.Add(Node.NodeId, WorkingNode);
	}

	FOccupancyResolver ExecOccupancy(RuleSet);
	ReserveImmovableExistingNodes(Snapshot, RuleSet, ExecOccupancy);

	TArray<FString> Roots;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		const int32 InputCount = Topology.CountExecInputs(Node.NodeId);
		if (Role == ENodeRole::EventEntry || (IsExecRole(Role) && InputCount == 0))
		{
			Roots.Add(Node.NodeId);
		}
	}

	TSet<FString> VisitedExecNodes;
	TMap<int32, TSet<int32>> UsedRows;
	for (int32 RootIndex = 0; RootIndex < Roots.Num(); ++RootIndex)
	{
		LayoutExecChain(Roots[RootIndex], Topology, Nodes, RuleSet, ExecOccupancy, UsedRows, RootIndex * 3, VisitedExecNodes);
	}

	int32 DetachedRootRow = FMath::Max(1, Roots.Num()) * 3;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		if (IsExecRole(Role) && !VisitedExecNodes.Contains(Node.NodeId))
		{
			while (UsedRows.FindOrAdd(0).Contains(DetachedRootRow))
			{
				++DetachedRootRow;
			}
			LayoutExecChain(Node.NodeId, Topology, Nodes, RuleSet, ExecOccupancy, UsedRows, DetachedRootRow, VisitedExecNodes);
			DetachedRootRow += 3;
		}
	}

	if (RuleSet.bUsePatternRowHeightBudget)
	{
		const TMap<int32, float> BaselinesByRow = BuildAllocatedRowBaselines(Snapshot, Topology, Nodes, RuleSet);

		FOccupancyResolver PatternOccupancy(RuleSet);
		ReserveImmovableExistingNodes(Snapshot, RuleSet, PatternOccupancy);
		ReflowExecTargetsToAllocatedRows(Snapshot, BaselinesByRow, Nodes, RuleSet, PatternOccupancy);

		if (RuleSet.bUsePureDataSubgraphLayout)
		{
			PlaceInputClusters(Snapshot, Topology, Nodes, RuleSet, PatternOccupancy);
		}
		else
		{
			for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
			{
				if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, PatternOccupancy))
				{
					break;
				}
			}
		}
	}
	else if (RuleSet.bUsePureDataSubgraphLayout)
	{
		PlaceInputClusters(Snapshot, Topology, Nodes, RuleSet, ExecOccupancy);
	}
	else
	{
		for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
		{
			if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, ExecOccupancy))
			{
				break;
			}
		}
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const FWorkingNode* WorkingNode = Nodes.Find(Node.NodeId);
		FNodePlacement Placement;
		Placement.NodeId = Node.NodeId;
		Placement.Role = WorkingNode ? WorkingNode->Role : ENodeRole::Unknown;
		Placement.CurrentPosition = Node.Position;
		Placement.TargetPosition = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Target : Node.Position;
		Placement.bMoveExisting = (!Node.bExisting && RuleSet.bMoveGeneratedNodes) ||
			(Node.bExisting && RuleSet.bMoveExistingNodes);
		Placement.Reason = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Reason : TEXT("no_target_generated");
		Plan.Placements.Add(Placement);
	}

	return Plan;
}
}
