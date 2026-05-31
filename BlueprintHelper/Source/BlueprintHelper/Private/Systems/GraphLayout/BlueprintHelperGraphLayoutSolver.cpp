#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"

namespace BlueprintHelper::GraphLayout
{
struct FWorkingNode
{
	const FNodeSnapshot* Snapshot = nullptr;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Target = FVector2D::ZeroVector;
	bool bHasTarget = false;
	bool bPinnedToCurrentPosition = false;
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

static TArray<FString> GetExecSuccessors(const FNodeSnapshot& Node)
{
	TArray<FString> Result;
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Direction == EPinDirection::Output && Pin.bExec)
		{
			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				Result.AddUnique(LinkedNodeId);
			}
		}
	}
	return Result;
}

static TMap<FString, int32> CountExecInputs(const FGraphSnapshot& Snapshot)
{
	TMap<FString, int32> Counts;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Counts.FindOrAdd(Node.NodeId) = 0;
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		for (const FString& Successor : GetExecSuccessors(Node))
		{
			++Counts.FindOrAdd(Successor);
		}
	}
	return Counts;
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

static void LayoutExecChain(
	const FString& RootNodeId,
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

		const TArray<FString> Successors = GetExecSuccessors(*Node->Snapshot);
		for (int32 SuccessorIndex = 0; SuccessorIndex < Successors.Num(); ++SuccessorIndex)
		{
			const FWorkingNode* Successor = Nodes.Find(Successors[SuccessorIndex]);
			if (!Successor || !IsExecRole(Successor->Role))
			{
				continue;
			}

			const int32 BranchOffset = Node->Role == ENodeRole::BranchControl ? SuccessorIndex : 0;
			const float RowStep = Node->Role == ENodeRole::BranchControl
				? RuleSet.BranchRowSpacing / FMath::Max(1.0f, RuleSet.ExecRowSpacing)
				: 1.0f;
			Queue.Add({ Successors[SuccessorIndex], Item.Column + 1, ResolvedRow + FMath::RoundToInt(BranchOffset * RowStep) });
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

FLayoutPlan FSolver::Solve(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet)
{
	FLayoutPlan Plan;
	Plan.Classifications = FClassifier::ClassifyGraph(Snapshot, RuleSet);

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

	FOccupancyResolver Occupancy(RuleSet);
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (Node.bExisting && !RuleSet.bMoveExistingNodes)
		{
			Occupancy.ReserveExistingNode(Node);
		}
	}

	const TMap<FString, int32> ExecInputCounts = CountExecInputs(Snapshot);
	TArray<FString> Roots;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		const int32 InputCount = ExecInputCounts.FindRef(Node.NodeId);
		if (Role == ENodeRole::EventEntry || (IsExecRole(Role) && InputCount == 0))
		{
			Roots.Add(Node.NodeId);
		}
	}

	TSet<FString> VisitedExecNodes;
	TMap<int32, TSet<int32>> UsedRows;
	for (int32 RootIndex = 0; RootIndex < Roots.Num(); ++RootIndex)
	{
		LayoutExecChain(Roots[RootIndex], Nodes, RuleSet, Occupancy, UsedRows, RootIndex * 3, VisitedExecNodes);
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
			LayoutExecChain(Node.NodeId, Nodes, RuleSet, Occupancy, UsedRows, DetachedRootRow, VisitedExecNodes);
			DetachedRootRow += 3;
		}
	}

	for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
	{
		if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, Occupancy))
		{
			break;
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
