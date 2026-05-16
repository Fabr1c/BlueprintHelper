#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"

namespace BlueprintHelper::GraphLayout
{
struct FWorkingNode
{
	const FNodeSnapshot* Snapshot = nullptr;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Target = FVector2D::ZeroVector;
	bool bHasTarget = false;
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

static const FNodeSnapshot* FindNode(const TMap<FString, FWorkingNode>& Nodes, const FString& NodeId)
{
	const FWorkingNode* Node = Nodes.Find(NodeId);
	return Node ? Node->Snapshot : nullptr;
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

static void SetTarget(
	TMap<FString, FWorkingNode>& Nodes,
	const FString& NodeId,
	const FVector2D& Target,
	const FString& Reason)
{
	if (FWorkingNode* Node = Nodes.Find(NodeId))
	{
		Node->Target = Target;
		Node->bHasTarget = true;
		Node->Reason = Reason;
	}
}

static void LayoutExecChain(
	const FString& RootNodeId,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
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
	TMap<int32, TSet<int32>> UsedRows;
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
		SetTarget(Nodes, Item.NodeId, Target, TEXT("exec_flow"));

		const TArray<FString> Successors = GetExecSuccessors(*Node->Snapshot);
		for (int32 SuccessorIndex = 0; SuccessorIndex < Successors.Num(); ++SuccessorIndex)
		{
			const FWorkingNode* Successor = Nodes.Find(Successors[SuccessorIndex]);
			if (!Successor || !IsExecRole(Successor->Role))
			{
				continue;
			}

			const int32 BranchOffset = Node->Role == ENodeRole::BranchControl ? SuccessorIndex : 0;
			const float RowStep = Node->Role == ENodeRole::BranchControl ? RuleSet.BranchRowSpacing / RuleSet.ExecRowSpacing : 1.0f;
			Queue.Add({ Successors[SuccessorIndex], Item.Column + 1, Row + FMath::RoundToInt(BranchOffset * RowStep) });
		}
	}
}

static void AlignInputsToTargetPinOrder(TMap<FString, FWorkingNode>& Nodes, const FRuleSet& RuleSet)
{
	for (auto& Pair : Nodes)
	{
		FWorkingNode& TargetNode = Pair.Value;
		if (!TargetNode.Snapshot || !TargetNode.bHasTarget)
		{
			continue;
		}

		int32 InputOrder = 0;
		for (const FPinSnapshot& Pin : TargetNode.Snapshot->Pins)
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

				if (SourceNode->Role == ENodeRole::VariableInput)
				{
					const FVector2D Target(
						TargetNode.Target.X - RuleSet.VariableInputOffsetX,
						TargetNode.Target.Y + InputOrder * RuleSet.InputPinRowSpacing);
					SetTarget(Nodes, LinkedNodeId, Target, TEXT("target_pin_order_variable_input_alignment"));
				}
				else if (SourceNode->Role == ENodeRole::PureFunction && !SourceNode->bHasTarget)
				{
					const FVector2D Target(
						TargetNode.Target.X - RuleSet.PureInputOffsetX,
						TargetNode.Target.Y + InputOrder * RuleSet.InputPinRowSpacing);
					SetTarget(Nodes, LinkedNodeId, Target, TEXT("pure_input_alignment"));
				}
			}
			++InputOrder;
		}
	}
}

static void StraightenExistingReroutes(TMap<FString, FWorkingNode>& Nodes)
{
	for (auto& Pair : Nodes)
	{
		FWorkingNode& Reroute = Pair.Value;
		if (!Reroute.Snapshot || Reroute.Role != ENodeRole::Reroute || !Reroute.Snapshot->bExisting)
		{
			continue;
		}

		TArray<FVector2D> NeighborTargets;
		for (const FPinSnapshot& Pin : Reroute.Snapshot->Pins)
		{
			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				const FWorkingNode* LinkedNode = Nodes.Find(LinkedNodeId);
				if (LinkedNode && LinkedNode->bHasTarget && LinkedNode->Role != ENodeRole::Reroute)
				{
					NeighborTargets.Add(LinkedNode->Target);
				}
			}
		}

		if (NeighborTargets.Num() == 0)
		{
			continue;
		}

		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& Target : NeighborTargets)
		{
			Sum += Target;
		}
		SetTarget(Nodes, Reroute.Snapshot->NodeId, Sum / static_cast<float>(NeighborTargets.Num()), TEXT("straighten_existing_reroute"));
	}
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
		Nodes.Add(Node.NodeId, WorkingNode);
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
	for (int32 RootIndex = 0; RootIndex < Roots.Num(); ++RootIndex)
	{
		LayoutExecChain(Roots[RootIndex], Nodes, RuleSet, RootIndex * 3, VisitedExecNodes);
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		if (IsExecRole(Role) && !VisitedExecNodes.Contains(Node.NodeId))
		{
			LayoutExecChain(Node.NodeId, Nodes, RuleSet, FMath::Max(1, VisitedExecNodes.Num()) * 2, VisitedExecNodes);
		}
	}

	if (RuleSet.bUseTargetPinOrderForVariableInputs)
	{
		AlignInputsToTargetPinOrder(Nodes, RuleSet);
	}

	if (RuleSet.bStraightenExistingReroutes)
	{
		StraightenExistingReroutes(Nodes);
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const FWorkingNode* WorkingNode = Nodes.Find(Node.NodeId);
		FNodePlacement Placement;
		Placement.NodeId = Node.NodeId;
		Placement.Role = WorkingNode ? WorkingNode->Role : ENodeRole::Unknown;
		Placement.CurrentPosition = Node.Position;
		Placement.TargetPosition = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Target : Node.Position;
		Placement.bStraightenExistingReroute = WorkingNode && WorkingNode->Role == ENodeRole::Reroute && WorkingNode->bHasTarget && WorkingNode->Reason == TEXT("straighten_existing_reroute");
		Placement.bMoveExisting = (!Node.bExisting && RuleSet.bMoveGeneratedNodes) ||
			(Node.bExisting && RuleSet.bMoveExistingNodes) ||
			Placement.bStraightenExistingReroute;
		Placement.Reason = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Reason : TEXT("no_target_generated");
		Plan.Placements.Add(Placement);
	}

	return Plan;
}
}
