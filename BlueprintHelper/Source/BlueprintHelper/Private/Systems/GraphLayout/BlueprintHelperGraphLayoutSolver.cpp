#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutArrangeScopePolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutExecPinAnchor.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutGroupAvoidancePolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutLayeredComponentPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPriorityCollisionResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutQualityGate.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

namespace BlueprintHelper::GraphLayout
{
struct FWorkingNode
{
	const FNodeSnapshot* Snapshot = nullptr;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Target = FVector2D::ZeroVector;
	FVector2D PreferredTarget = FVector2D::ZeroVector;
	bool bHasTarget = false;
	bool bHasPreferredTarget = false;
	bool bPinnedToCurrentPosition = false;
	int32 SemanticRow = INDEX_NONE;
	int32 ExecColumn = INDEX_NONE;
	FString LayoutGroupId;
	FString Reason;
};

enum class ETargetCollisionPolicy : uint8
{
	DownwardOnly,
	PreferSameRow,
	PreferSameRowLeft
};

static bool IsExecRole(ENodeRole Role)
{
	return Role == ENodeRole::EventEntry ||
		Role == ENodeRole::ExecNode ||
		Role == ENodeRole::BranchControl ||
		Role == ENodeRole::AsyncNode ||
		Role == ENodeRole::DelegateNode;
}

static ETargetCollisionPolicy GetExecCollisionPolicy(const FRuleSet& RuleSet, int32 ExecColumn)
{
	return RuleSet.bAlignExecNodesHorizontally && ExecColumn >= 0
		? ETargetCollisionPolicy::PreferSameRow
		: ETargetCollisionPolicy::DownwardOnly;
}

static EGraphLayoutCollisionSearchMode ToSearchMode(const ETargetCollisionPolicy CollisionPolicy)
{
	return CollisionPolicy == ETargetCollisionPolicy::PreferSameRow ||
			CollisionPolicy == ETargetCollisionPolicy::PreferSameRowLeft
		? EGraphLayoutCollisionSearchMode::PreferSameRow
		: EGraphLayoutCollisionSearchMode::DownwardOnly;
}

static int32 ResolveHorizontalDirection(const ETargetCollisionPolicy CollisionPolicy)
{
	return CollisionPolicy == ETargetCollisionPolicy::PreferSameRowLeft ? -1 : 1;
}

static FString ResolveExplicitLayoutGroupId(const FNodeSnapshot& Node)
{
	if (!Node.LayoutBlockId.IsEmpty())
	{
		return Node.LayoutBlockId;
	}
	return Node.bExisting
		? FString::Printf(TEXT("existing_node:%s"), *Node.NodeId)
		: FString();
}

static TMap<FString, FString> BuildFallbackGeneratedLayoutGroupIds(
	const FGraphSnapshot& Snapshot,
	const TMap<FString, ENodeRole>& RolesById)
{
	TMap<FString, TSet<FString>> AdjacencyByNodeId;
	TMap<FString, int32> OrderByNodeId;
	TSet<FString> GeneratedNodesWithoutExplicitGroup;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		OrderByNodeId.Add(Node.NodeId, OrderByNodeId.Num());
		if (!Node.bExisting && Node.LayoutBlockId.IsEmpty())
		{
			GeneratedNodesWithoutExplicitGroup.Add(Node.NodeId);
			AdjacencyByNodeId.FindOrAdd(Node.NodeId);
		}
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (!GeneratedNodesWithoutExplicitGroup.Contains(Node.NodeId))
		{
			continue;
		}

		for (const FPinSnapshot& Pin : Node.Pins)
		{
			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				if (!GeneratedNodesWithoutExplicitGroup.Contains(LinkedNodeId))
				{
					continue;
				}
				AdjacencyByNodeId.FindOrAdd(Node.NodeId).Add(LinkedNodeId);
				AdjacencyByNodeId.FindOrAdd(LinkedNodeId).Add(Node.NodeId);
			}
		}
	}

	TMap<FString, FString> GroupIdByNodeId;
	TSet<FString> Visited;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (!GeneratedNodesWithoutExplicitGroup.Contains(Node.NodeId) || Visited.Contains(Node.NodeId))
		{
			continue;
		}

		TArray<FString> ComponentNodes;
		TArray<FString> Queue;
		Queue.Add(Node.NodeId);
		Visited.Add(Node.NodeId);
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const FString CurrentNodeId = Queue[QueueIndex];
			ComponentNodes.Add(CurrentNodeId);
			const TSet<FString>* Neighbors = AdjacencyByNodeId.Find(CurrentNodeId);
			if (!Neighbors)
			{
				continue;
			}
			for (const FString& Neighbor : *Neighbors)
			{
				if (!Visited.Contains(Neighbor))
				{
					Visited.Add(Neighbor);
					Queue.Add(Neighbor);
				}
			}
		}

		ComponentNodes.Sort([&OrderByNodeId](const FString& Left, const FString& Right)
		{
			return OrderByNodeId.FindRef(Left) < OrderByNodeId.FindRef(Right);
		});

		FString AnchorNodeId = ComponentNodes.Num() > 0 ? ComponentNodes[0] : Node.NodeId;
		for (const FString& ComponentNodeId : ComponentNodes)
		{
			if (RolesById.FindRef(ComponentNodeId) == ENodeRole::EventEntry)
			{
				AnchorNodeId = ComponentNodeId;
				break;
			}
		}

		const FString GroupId = FString::Printf(TEXT("entry:%s"), *AnchorNodeId);
		for (const FString& ComponentNodeId : ComponentNodes)
		{
			GroupIdByNodeId.Add(ComponentNodeId, GroupId);
		}
	}

	return GroupIdByNodeId;
}

static FVector2D ResolveTargetWithPolicy(
	FOccupancyResolver& Occupancy,
	const FString& NodeId,
	const FString& LayoutGroupId,
	const FVector2D& DesiredTarget,
	const FVector2D& Size,
	const ETargetCollisionPolicy CollisionPolicy)
{
	FResolveTargetRequest Request;
	Request.NodeId = NodeId;
	Request.LayoutGroupId = LayoutGroupId;
	Request.DesiredPosition = DesiredTarget;
	Request.Size = Size;
	Request.SearchMode = ToSearchMode(CollisionPolicy);
	Request.PreferredHorizontalDirection = ResolveHorizontalDirection(CollisionPolicy);
	Request.bIgnoreSameGroupReservations = CollisionPolicy == ETargetCollisionPolicy::DownwardOnly;
	Request.bIgnoreExternalGroupReservations = !LayoutGroupId.IsEmpty();
	return Occupancy.ResolveTarget(Request);
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

static float GetExecAnchorOffsetY(const FWorkingNode& Node)
{
	return Node.Snapshot
		? FGraphLayoutExecPinAnchor::GetPrimaryExecAnchorOffsetY(*Node.Snapshot, Node.Role)
		: 48.0f;
}

static float GetExecBaselineY(const FWorkingNode& Node)
{
	return Node.Target.Y + GetExecAnchorOffsetY(Node);
}

static FVector2D BuildExecTopLeftFromBaseline(
	const FWorkingNode& Node,
	const float TargetX,
	const float BaselineY)
{
	return FVector2D(TargetX, BaselineY - GetExecAnchorOffsetY(Node));
}

static int32 GetRowForExecBaselineY(const FWorkingNode& Node, const FRuleSet& RuleSet)
{
	const float RowSpacing = FMath::Max(1.0f, RuleSet.ExecRowSpacing);
	return FMath::CeilToInt(GetExecBaselineY(Node) / RowSpacing);
}

static void SortNodeIdsByLayoutPriority(
	TArray<FString>& NodeIds,
	const TMap<FString, FWorkingNode>& Nodes,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	NodeIds.Sort([&Nodes, &SnapshotOrderByNodeId](const FString& LeftNodeId, const FString& RightNodeId)
	{
		const FWorkingNode* LeftNode = Nodes.Find(LeftNodeId);
		const FWorkingNode* RightNode = Nodes.Find(RightNodeId);
		const FNodeSnapshot* LeftSnapshot = LeftNode ? LeftNode->Snapshot : nullptr;
		const FNodeSnapshot* RightSnapshot = RightNode ? RightNode->Snapshot : nullptr;
		const bool bLeftHasBlockOrder = LeftSnapshot && LeftSnapshot->LayoutBlockOrder != INDEX_NONE;
		const bool bRightHasBlockOrder = RightSnapshot && RightSnapshot->LayoutBlockOrder != INDEX_NONE;
		if (bLeftHasBlockOrder != bRightHasBlockOrder)
		{
			return bLeftHasBlockOrder;
		}
		if (bLeftHasBlockOrder && LeftSnapshot->LayoutBlockOrder != RightSnapshot->LayoutBlockOrder)
		{
			return LeftSnapshot->LayoutBlockOrder < RightSnapshot->LayoutBlockOrder;
		}

		if (LeftNode && RightNode && LeftNode->ExecColumn != RightNode->ExecColumn)
		{
			if (LeftNode->ExecColumn != INDEX_NONE && RightNode->ExecColumn != INDEX_NONE)
			{
				return LeftNode->ExecColumn < RightNode->ExecColumn;
			}
			return LeftNode->ExecColumn != INDEX_NONE;
		}

		if (LeftNode && RightNode && LeftNode->SemanticRow != RightNode->SemanticRow)
		{
			if (LeftNode->SemanticRow != INDEX_NONE && RightNode->SemanticRow != INDEX_NONE)
			{
				return LeftNode->SemanticRow < RightNode->SemanticRow;
			}
			return LeftNode->SemanticRow != INDEX_NONE;
		}

		const bool bLeftHasNodeOrder = LeftSnapshot && LeftSnapshot->LayoutNodeOrder != INDEX_NONE;
		const bool bRightHasNodeOrder = RightSnapshot && RightSnapshot->LayoutNodeOrder != INDEX_NONE;
		if (bLeftHasNodeOrder != bRightHasNodeOrder)
		{
			return bLeftHasNodeOrder;
		}
		if (bLeftHasNodeOrder && LeftSnapshot->LayoutNodeOrder != RightSnapshot->LayoutNodeOrder)
		{
			return LeftSnapshot->LayoutNodeOrder < RightSnapshot->LayoutNodeOrder;
		}

		return SnapshotOrderByNodeId.FindRef(LeftNodeId) < SnapshotOrderByNodeId.FindRef(RightNodeId);
	});
}

static void SetTarget(
	TMap<FString, FWorkingNode>& Nodes,
	FOccupancyResolver& Occupancy,
	const FString& NodeId,
	const FVector2D& DesiredTarget,
	const FString& Reason,
	const ETargetCollisionPolicy CollisionPolicy = ETargetCollisionPolicy::DownwardOnly)
{
	if (FWorkingNode* Node = Nodes.Find(NodeId))
	{
		if (Node->bPinnedToCurrentPosition)
		{
			if (Node->Snapshot && !Node->bHasTarget)
			{
				Node->Target = Node->Snapshot->Position;
				Node->PreferredTarget = Node->Snapshot->Position;
				Node->bHasTarget = true;
				Node->bHasPreferredTarget = true;
				Node->Reason = TEXT("existing_node_static_anchor");
			}
			return;
		}

		const FVector2D Size = Node->Snapshot ? Node->Snapshot->Size : FVector2D(180.0f, 80.0f);
		Node->PreferredTarget = DesiredTarget;
		Node->bHasPreferredTarget = true;
		const FVector2D Target = ResolveTargetWithPolicy(
			Occupancy,
			NodeId,
			Node->LayoutGroupId,
			DesiredTarget,
			Size,
			CollisionPolicy);
		Node->Target = Target;
		Node->bHasTarget = true;
		Node->Reason = Target.Equals(DesiredTarget)
			? Reason
			: FString::Printf(TEXT("%s_avoided_overlap"), *Reason);
		Occupancy.ReserveTarget(NodeId, Target, Size, true, Node->LayoutGroupId);
	}
}

static void ReserveImmovableScopeNodes(
	const FGraphSnapshot& Snapshot,
	const FGraphLayoutArrangeScope& ArrangeScope,
	FOccupancyResolver& Occupancy)
{
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (!ArrangeScope.ToArrangeNodeIds.Contains(Node.NodeId))
		{
			Occupancy.ReserveExistingNode(Node);
		}
	}
}

static bool IsLayeredSeedReason(const FString& Reason)
{
	return Reason == TEXT("layered_component");
}

static void SeedLayeredTargets(
	TMap<FString, FWorkingNode>& Nodes,
	const FGraphLayoutLayeredResult& LayeredResult)
{
	for (const FGraphLayoutLayeredPlacement& LayeredPlacement : LayeredResult.Placements)
	{
		FWorkingNode* Node = Nodes.Find(LayeredPlacement.NodeId);
		if (!Node || Node->bPinnedToCurrentPosition)
		{
			continue;
		}

		Node->Target = LayeredPlacement.TargetPosition;
		Node->PreferredTarget = LayeredPlacement.TargetPosition;
		Node->bHasTarget = true;
		Node->bHasPreferredTarget = true;
		Node->Reason = TEXT("layered_component");
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
		Node->ExecColumn = Item.Column;
		const FVector2D Target = BuildExecTopLeftFromBaseline(
			*Node,
			Item.Column * RuleSet.ExecColumnSpacing,
			Row * RuleSet.ExecRowSpacing);
		SetTarget(
			Nodes,
			Occupancy,
			Item.NodeId,
			Target,
			TEXT("exec_flow"),
			GetExecCollisionPolicy(RuleSet, Item.Column));
		const int32 ResolvedRow = GetRowForExecBaselineY(*Node, RuleSet);
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
	FOccupancyResolver& Occupancy,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	bool bChanged = false;
	TArray<FString> ConsumerNodeIds;
	for (const FNodeSnapshot& ConsumerSnapshot : Snapshot.Nodes)
	{
		const FWorkingNode* ConsumerNode = Nodes.Find(ConsumerSnapshot.NodeId);
		if (ConsumerNode && ConsumerNode->bHasTarget)
		{
			ConsumerNodeIds.Add(ConsumerSnapshot.NodeId);
		}
	}
	SortNodeIdsByLayoutPriority(ConsumerNodeIds, Nodes, SnapshotOrderByNodeId);

	for (const FString& ConsumerNodeId : ConsumerNodeIds)
	{
		const FNodeSnapshot* ConsumerSnapshot = Snapshot.Nodes.FindByPredicate([&ConsumerNodeId](const FNodeSnapshot& Node)
		{
			return Node.NodeId == ConsumerNodeId;
		});
		FWorkingNode* ConsumerNode = Nodes.Find(ConsumerNodeId);
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

				if (FDataInputPlacement::IsDataInputRole(SourceNode->Role) &&
					(!SourceNode->bHasTarget || IsLayeredSeedReason(SourceNode->Reason)))
				{
					const int32 PlacementOrder =
						SourceNode->Role == ENodeRole::VariableInput && !RuleSet.bUseTargetPinOrderForVariableInputs
							? 0
							: InputOrder;

					FDataInputPlacementRequest Request;
					Request.ConsumerNodeId = ConsumerSnapshot ? ConsumerSnapshot->NodeId : ConsumerNodeId;
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
						FDataInputPlacement::GetReason(SourceNode->Role),
						ETargetCollisionPolicy::PreferSameRowLeft);
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

				const FVector2D ResolvedTarget = ResolveTargetWithPolicy(
					Occupancy,
					NodeId,
					Node->LayoutGroupId,
					BuildExecTopLeftFromBaseline(*Node, Node->Target.X, ResolvedBaselineY),
					Node->Snapshot->Size,
					GetExecCollisionPolicy(RuleSet, Node->ExecColumn));
				NextBaselineY = FMath::Max(NextBaselineY, ResolvedTarget.Y + GetExecAnchorOffsetY(*Node));
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
				BuildExecTopLeftFromBaseline(*Node, Node->Target.X, ResolvedBaselineY),
				TEXT("exec_flow"),
				GetExecCollisionPolicy(RuleSet, Node->ExecColumn));

			if (FWorkingNode* UpdatedNode = Nodes.Find(NodeId))
			{
				FinalBaselineY = FMath::Max(FinalBaselineY, GetExecBaselineY(*UpdatedNode));
				UpdatedNode->SemanticRow = GetRowForExecBaselineY(*UpdatedNode, RuleSet);
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
	FOccupancyResolver& Occupancy,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	bool bChanged = false;
	TArray<FString> ConsumerNodeIds;
	for (const FNodeSnapshot& ConsumerSnapshot : Snapshot.Nodes)
	{
		const FWorkingNode* ConsumerNode = Nodes.Find(ConsumerSnapshot.NodeId);
		if (ConsumerNode && ConsumerNode->bHasTarget)
		{
			ConsumerNodeIds.Add(ConsumerSnapshot.NodeId);
		}
	}
	SortNodeIdsByLayoutPriority(ConsumerNodeIds, Nodes, SnapshotOrderByNodeId);

	for (const FString& ConsumerNodeId : ConsumerNodeIds)
	{
		const FWorkingNode* ConsumerNode = Nodes.Find(ConsumerNodeId);
		if (!ConsumerNode || !ConsumerNode->bHasTarget)
		{
			continue;
		}

		const FNodeInputClusterBudget ClusterBudget =
			FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, ConsumerNodeId, RuleSet);
		TArray<FString> ClusterNodeIds = ClusterBudget.NodeIds;
		ClusterNodeIds.Sort([&ClusterBudget, &SnapshotOrderByNodeId](const FString& Left, const FString& Right)
		{
			const float LeftPriority = ClusterBudget.LayoutPriorityByNodeId.FindRef(Left);
			const float RightPriority = ClusterBudget.LayoutPriorityByNodeId.FindRef(Right);
			if (!FMath::IsNearlyEqual(LeftPriority, RightPriority))
			{
				return LeftPriority > RightPriority;
			}
			return SnapshotOrderByNodeId.FindRef(Left) < SnapshotOrderByNodeId.FindRef(Right);
		});

		for (const FString& ClusterNodeId : ClusterNodeIds)
		{
			FWorkingNode* ClusterNode = Nodes.Find(ClusterNodeId);
			if (!ClusterNode ||
				!ClusterNode->Snapshot ||
				(ClusterNode->bHasTarget && !IsLayeredSeedReason(ClusterNode->Reason)))
			{
				continue;
			}

			const FVector2D* RelativeTarget = ClusterBudget.RelativeTargets.Find(ClusterNodeId);
			if (!RelativeTarget)
			{
				continue;
			}

			const EPureDataNodeKind ClusterKind = ClusterBudget.KindByNodeId.FindRef(ClusterNodeId);
			const ETargetCollisionPolicy DataPolicy = ClusterKind == EPureDataNodeKind::None
				? ETargetCollisionPolicy::DownwardOnly
				: ETargetCollisionPolicy::PreferSameRowLeft;
			SetTarget(
				Nodes,
				Occupancy,
				ClusterNodeId,
				ConsumerNode->Target + *RelativeTarget,
				TEXT("pure_data_subgraph_alignment"),
				DataPolicy);
			bChanged = true;
		}
	}
	return bChanged;
}

static TArray<FGraphLayoutGroupNode> BuildGroupAvoidanceNodes(
	const FGraphSnapshot& Snapshot,
	const TMap<FString, FWorkingNode>& Nodes,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	TArray<FGraphLayoutGroupNode> GroupNodes;
	for (const FNodeSnapshot& SnapshotNode : Snapshot.Nodes)
	{
		const FWorkingNode* Node = Nodes.Find(SnapshotNode.NodeId);
		if (!Node || !Node->bHasTarget)
		{
			continue;
		}

		const bool bPinnedExistingNode = Node->bPinnedToCurrentPosition;
		if (bPinnedExistingNode && SnapshotNode.LayoutBlockId.IsEmpty())
		{
			continue;
		}
		FGraphLayoutGroupNode& GroupNode = GroupNodes.AddDefaulted_GetRef();
		GroupNode.NodeId = SnapshotNode.NodeId;
		GroupNode.LayoutGroupId = bPinnedExistingNode ? SnapshotNode.LayoutBlockId : Node->LayoutGroupId;
		GroupNode.LayoutGroupOrder = SnapshotNode.LayoutBlockOrder;
		GroupNode.NodeOrder = SnapshotNode.LayoutNodeOrder != INDEX_NONE
			? SnapshotNode.LayoutNodeOrder
			: SnapshotOrderByNodeId.FindRef(SnapshotNode.NodeId);
		GroupNode.TargetPosition = Node->Target;
		GroupNode.Size = SnapshotNode.Size;
		GroupNode.bGenerated = !bPinnedExistingNode;
	}
	return GroupNodes;
}

static void ApplyGroupOffsets(
	TMap<FString, FWorkingNode>& Nodes,
	const TArray<FGraphLayoutGroupOffset>& GroupOffsets)
{
	TMap<FString, FGraphLayoutGroupOffset> OffsetsByGroupId;
	for (const FGraphLayoutGroupOffset& Offset : GroupOffsets)
	{
		OffsetsByGroupId.Add(Offset.LayoutGroupId, Offset);
	}

	for (TPair<FString, FWorkingNode>& Pair : Nodes)
	{
		FWorkingNode& Node = Pair.Value;
		if (!Node.bHasTarget || Node.bPinnedToCurrentPosition)
		{
			continue;
		}

		const FGraphLayoutGroupOffset* Offset = OffsetsByGroupId.Find(Node.LayoutGroupId);
		if (!Offset || Offset->Offset.IsNearlyZero())
		{
			continue;
		}

		Node.Target += Offset->Offset;
		if (Node.bHasPreferredTarget)
		{
			Node.PreferredTarget += Offset->Offset;
		}
		Node.Reason = Node.Reason.IsEmpty()
			? Offset->Reason
			: FString::Printf(TEXT("%s|%s"), *Node.Reason, *Offset->Reason);
	}
}

static float ResolveRoleCollisionPriority(const ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::EventEntry:
		return 100.0f;
	case ENodeRole::ExecNode:
	case ENodeRole::BranchControl:
		return 90.0f;
	case ENodeRole::AsyncNode:
	case ENodeRole::DelegateNode:
		return 85.0f;
	case ENodeRole::PureFunction:
		return 60.0f;
	case ENodeRole::OperatorOrCompare:
		return 55.0f;
	case ENodeRole::VariableInput:
		return 50.0f;
	case ENodeRole::Comment:
		return 10.0f;
	default:
		return 0.0f;
	}
}

static TMap<FString, float> BuildCollisionPriorities(
	const FGraphSnapshot& Snapshot,
	const TMap<FString, FWorkingNode>& Nodes,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	TMap<FString, float> PrioritiesByNodeId;
	for (const FNodeSnapshot& SnapshotNode : Snapshot.Nodes)
	{
		const FWorkingNode* Node = Nodes.Find(SnapshotNode.NodeId);
		float Priority = ResolveRoleCollisionPriority(Node ? Node->Role : ENodeRole::Unknown);
		if (SnapshotNode.LayoutBlockOrder != INDEX_NONE)
		{
			Priority += 1.0f / static_cast<float>(SnapshotNode.LayoutBlockOrder + 2);
		}
		if (SnapshotNode.LayoutNodeOrder != INDEX_NONE)
		{
			Priority += 0.01f / static_cast<float>(SnapshotNode.LayoutNodeOrder + 2);
		}
		Priority -= static_cast<float>(SnapshotOrderByNodeId.FindRef(SnapshotNode.NodeId)) * 0.0001f;
		PrioritiesByNodeId.Add(SnapshotNode.NodeId, Priority);
	}

	for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
	{
		bool bChanged = false;
		for (const FNodeSnapshot& SourceNode : Snapshot.Nodes)
		{
			for (const FPinSnapshot& Pin : SourceNode.Pins)
			{
				if (Pin.Direction != EPinDirection::Output || Pin.bExec)
				{
					continue;
				}

				for (const FString& TargetNodeId : Pin.LinkedNodeIds)
				{
					const float TargetPriority = PrioritiesByNodeId.FindRef(TargetNodeId);
					const float CandidatePriority = TargetPriority - 0.1f;
					float& SourcePriority = PrioritiesByNodeId.FindOrAdd(SourceNode.NodeId);
					if (CandidatePriority > SourcePriority + KINDA_SMALL_NUMBER)
					{
						SourcePriority = CandidatePriority;
						bChanged = true;
					}
				}
			}
		}

		if (!bChanged)
		{
			break;
		}
	}

	return PrioritiesByNodeId;
}

static TArray<FGraphLayoutCollisionNode> BuildPriorityCollisionNodes(
	const FGraphSnapshot& Snapshot,
	const TMap<FString, FWorkingNode>& Nodes,
	const FGraphLayoutArrangeScope& ArrangeScope,
	const TMap<FString, int32>& SnapshotOrderByNodeId)
{
	const TMap<FString, float> PrioritiesByNodeId =
		BuildCollisionPriorities(Snapshot, Nodes, SnapshotOrderByNodeId);

	TArray<FGraphLayoutCollisionNode> CollisionNodes;
	CollisionNodes.Reserve(Snapshot.Nodes.Num());
	for (const FNodeSnapshot& SnapshotNode : Snapshot.Nodes)
	{
		const FWorkingNode* Node = Nodes.Find(SnapshotNode.NodeId);
		FGraphLayoutCollisionNode& CollisionNode = CollisionNodes.AddDefaulted_GetRef();
		CollisionNode.NodeId = SnapshotNode.NodeId;
		CollisionNode.LayoutGroupId = Node ? Node->LayoutGroupId : SnapshotNode.LayoutBlockId;
		CollisionNode.Role = Node ? Node->Role : ENodeRole::Unknown;
		CollisionNode.Position = Node && Node->bHasTarget
			? Node->Target
			: (Node && Node->bHasPreferredTarget ? Node->PreferredTarget : SnapshotNode.Position);
		CollisionNode.Size = SnapshotNode.Size;
		CollisionNode.Priority = PrioritiesByNodeId.FindRef(SnapshotNode.NodeId);
		CollisionNode.bMovable = ArrangeScope.ToArrangeNodeIds.Contains(SnapshotNode.NodeId);
		CollisionNode.StableOrder = SnapshotOrderByNodeId.FindRef(SnapshotNode.NodeId);
	}
	return CollisionNodes;
}

static void ApplyPriorityCollisionPositions(
	TMap<FString, FWorkingNode>& Nodes,
	const TMap<FString, FVector2D>& ResolvedPositions,
	const FGraphLayoutArrangeScope& ArrangeScope)
{
	for (TPair<FString, FWorkingNode>& Pair : Nodes)
	{
		if (!ArrangeScope.ToArrangeNodeIds.Contains(Pair.Key))
		{
			continue;
		}

		FWorkingNode& Node = Pair.Value;
		const FVector2D* ResolvedPosition = ResolvedPositions.Find(Pair.Key);
		if (!ResolvedPosition)
		{
			continue;
		}

		if (!Node.bHasTarget || !Node.Target.Equals(*ResolvedPosition))
		{
			Node.Target = *ResolvedPosition;
			Node.bHasTarget = true;
			Node.Reason = Node.Reason.IsEmpty()
				? TEXT("priority_collision_resolved")
				: FString::Printf(TEXT("%s|priority_collision_resolved"), *Node.Reason);
		}
	}
}

FLayoutPlan FSolver::Solve(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet)
{
	FLayoutPlan Plan;
	Plan.Classifications = FClassifier::ClassifyGraph(Snapshot, RuleSet);
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope ArrangeScope =
		FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);
	const FGraphLayoutLayeredResult LayeredResult =
		FGraphLayoutLayeredComponentPolicy::Layout(Snapshot, Topology, ArrangeScope, RuleSet);

	TMap<FString, ENodeRole> RolesById;
	for (const FNodeClassification& Classification : Plan.Classifications)
	{
		RolesById.Add(Classification.NodeId, Classification.Role);
	}

	TMap<FString, FWorkingNode> Nodes;
	TMap<FString, int32> SnapshotOrderByNodeId;
	const TMap<FString, FString> FallbackGeneratedGroupIds =
		BuildFallbackGeneratedLayoutGroupIds(Snapshot, RolesById);
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		FWorkingNode WorkingNode;
		WorkingNode.Snapshot = &Node;
		WorkingNode.Role = RolesById.FindRef(Node.NodeId);
		WorkingNode.LayoutGroupId = ResolveExplicitLayoutGroupId(Node);
		if (WorkingNode.LayoutGroupId.IsEmpty())
		{
			WorkingNode.LayoutGroupId = FallbackGeneratedGroupIds.FindRef(Node.NodeId);
		}
		WorkingNode.bPinnedToCurrentPosition = !ArrangeScope.ToArrangeNodeIds.Contains(Node.NodeId);
		if (WorkingNode.bPinnedToCurrentPosition)
		{
			WorkingNode.Target = Node.Position;
			WorkingNode.PreferredTarget = Node.Position;
			WorkingNode.bHasTarget = true;
			WorkingNode.bHasPreferredTarget = true;
			WorkingNode.Reason = TEXT("existing_node_static_anchor");
		}
		Nodes.Add(Node.NodeId, WorkingNode);
		SnapshotOrderByNodeId.Add(Node.NodeId, SnapshotOrderByNodeId.Num());
	}
	SeedLayeredTargets(Nodes, LayeredResult);

	FOccupancyResolver ExecOccupancy(RuleSet);
	ReserveImmovableScopeNodes(Snapshot, ArrangeScope, ExecOccupancy);

	TArray<FString> Roots;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		const int32 InputCount = Topology.CountExecInputs(Node.NodeId);
		const bool bParticipatesInLayout =
			ArrangeScope.ToArrangeNodeIds.Contains(Node.NodeId) ||
			ArrangeScope.AnchorNodeIds.Contains(Node.NodeId);
		if (bParticipatesInLayout && (Role == ENodeRole::EventEntry || (IsExecRole(Role) && InputCount == 0)))
		{
			Roots.Add(Node.NodeId);
		}
	}

	SortNodeIdsByLayoutPriority(Roots, Nodes, SnapshotOrderByNodeId);

	TSet<FString> VisitedExecNodes;
	TMap<int32, TSet<int32>> UsedRows;
	for (int32 RootIndex = 0; RootIndex < Roots.Num(); ++RootIndex)
	{
		LayoutExecChain(Roots[RootIndex], Topology, Nodes, RuleSet, ExecOccupancy, UsedRows, RootIndex * 3, VisitedExecNodes);
	}

	TArray<FString> DetachedRoots;
	int32 DetachedRootRow = FMath::Max(1, Roots.Num()) * 3;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const ENodeRole Role = RolesById.FindRef(Node.NodeId);
		if (IsExecRole(Role) &&
			ArrangeScope.ToArrangeNodeIds.Contains(Node.NodeId) &&
			!VisitedExecNodes.Contains(Node.NodeId))
		{
			DetachedRoots.Add(Node.NodeId);
		}
	}
	SortNodeIdsByLayoutPriority(DetachedRoots, Nodes, SnapshotOrderByNodeId);
	for (const FString& NodeId : DetachedRoots)
	{
		while (UsedRows.FindOrAdd(0).Contains(DetachedRootRow))
		{
			++DetachedRootRow;
		}
		LayoutExecChain(NodeId, Topology, Nodes, RuleSet, ExecOccupancy, UsedRows, DetachedRootRow, VisitedExecNodes);
		DetachedRootRow += 3;
	}

	if (RuleSet.bUsePatternRowHeightBudget)
	{
		const TMap<int32, float> BaselinesByRow = BuildAllocatedRowBaselines(Snapshot, Topology, Nodes, RuleSet);

		FOccupancyResolver PatternOccupancy(RuleSet);
		ReserveImmovableScopeNodes(Snapshot, ArrangeScope, PatternOccupancy);
		ReflowExecTargetsToAllocatedRows(Snapshot, BaselinesByRow, Nodes, RuleSet, PatternOccupancy);

		if (RuleSet.bUsePureDataSubgraphLayout)
		{
			PlaceInputClusters(Snapshot, Topology, Nodes, RuleSet, PatternOccupancy, SnapshotOrderByNodeId);
		}
		else
		{
			for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
			{
				if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, PatternOccupancy, SnapshotOrderByNodeId))
				{
					break;
				}
			}
		}
	}
	else if (RuleSet.bUsePureDataSubgraphLayout)
	{
		PlaceInputClusters(Snapshot, Topology, Nodes, RuleSet, ExecOccupancy, SnapshotOrderByNodeId);
	}
	else
	{
		for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
		{
			if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, ExecOccupancy, SnapshotOrderByNodeId))
			{
				break;
			}
		}
	}

	ApplyGroupOffsets(
		Nodes,
		FGraphLayoutGroupAvoidancePolicy::ResolveGroupOffsets(
			BuildGroupAvoidanceNodes(Snapshot, Nodes, SnapshotOrderByNodeId),
			RuleSet));

	ApplyPriorityCollisionPositions(
		Nodes,
		FGraphLayoutPriorityCollisionResolver::Resolve(
			BuildPriorityCollisionNodes(Snapshot, Nodes, ArrangeScope, SnapshotOrderByNodeId),
			RuleSet),
		ArrangeScope);

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		const FWorkingNode* WorkingNode = Nodes.Find(Node.NodeId);
		FNodePlacement Placement;
		Placement.NodeId = Node.NodeId;
		Placement.Role = WorkingNode ? WorkingNode->Role : ENodeRole::Unknown;
		Placement.CurrentPosition = Node.Position;
		Placement.TargetPosition = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Target : Node.Position;
		Placement.TargetSize = Node.Size;
		Placement.bMoveExisting = ArrangeScope.ToArrangeNodeIds.Contains(Node.NodeId);
		Placement.Reason = WorkingNode && WorkingNode->bHasTarget ? WorkingNode->Reason : TEXT("no_target_generated");
		Plan.Placements.Add(Placement);
	}

	for (const FString& Issue : LayeredResult.Issues)
	{
		Plan.Issues.Add(FString::Printf(TEXT("layered_component: %s"), *Issue));
	}
	const FGraphLayoutQualityResult Quality = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	for (const FGraphLayoutQualityIssue& Issue : Quality.Issues)
	{
		Plan.Issues.Add(FString::Printf(TEXT("%s: %s"), *Issue.Code, *Issue.Message));
	}

	return Plan;
}
}
