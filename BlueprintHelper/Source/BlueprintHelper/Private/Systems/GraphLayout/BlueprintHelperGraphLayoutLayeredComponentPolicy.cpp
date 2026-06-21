#include "Systems/GraphLayout/BlueprintHelperGraphLayoutLayeredComponentPolicy.h"

#include "Algo/Sort.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutLayeredComponent
{
	TSet<FString> NodeIds;
	TSet<FString> ArrangeNodeIds;
	TSet<FString> AnchorNodeIds;
	EGraphLayoutComponentKind Kind = EGraphLayoutComponentKind::FixedOnly;
};

struct FGraphLayoutLayeredBounds
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	bool bValid = false;

	bool Overlaps(const FGraphLayoutLayeredBounds& Other) const
	{
		return bValid && Other.bValid &&
			Min.X < Other.Max.X &&
			Max.X > Other.Min.X &&
			Min.Y < Other.Max.Y &&
			Max.Y > Other.Min.Y;
	}

	void Offset(const FVector2D& Delta)
	{
		Min += Delta;
		Max += Delta;
	}
};

struct FGraphLayoutLayeredAdjacency
{
	TMap<FString, TSet<FString>> Downstream;
	TMap<FString, TSet<FString>> Upstream;
	TMap<FString, TSet<FString>> Undirected;
};

struct FGraphLayoutLayeredComponentPolicyPrivate
{
	static const FNodeSnapshot* FindSnapshotNode(
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		const FString& NodeId)
	{
		if (const FNodeSnapshot* const* Node = NodesById.Find(NodeId))
		{
			return *Node;
		}
		return nullptr;
	}

	static FGraphLayoutLayeredAdjacency BuildAdjacency(const FGraphSnapshot& Snapshot)
	{
		FGraphLayoutLayeredAdjacency Adjacency;
		for (const FNodeSnapshot& Node : Snapshot.Nodes)
		{
			Adjacency.Downstream.FindOrAdd(Node.NodeId);
			Adjacency.Upstream.FindOrAdd(Node.NodeId);
			Adjacency.Undirected.FindOrAdd(Node.NodeId);
		}

		auto AddDirectedEdge = [&Adjacency](const FString& SourceNodeId, const FString& TargetNodeId)
		{
			if (SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty() ||
				!Adjacency.Downstream.Contains(SourceNodeId) ||
				!Adjacency.Downstream.Contains(TargetNodeId))
			{
				return;
			}
			Adjacency.Downstream.FindOrAdd(SourceNodeId).Add(TargetNodeId);
			Adjacency.Upstream.FindOrAdd(TargetNodeId).Add(SourceNodeId);
			Adjacency.Undirected.FindOrAdd(SourceNodeId).Add(TargetNodeId);
			Adjacency.Undirected.FindOrAdd(TargetNodeId).Add(SourceNodeId);
		};

		for (const FNodeSnapshot& Node : Snapshot.Nodes)
		{
			for (const FPinSnapshot& Pin : Node.Pins)
			{
				if (Pin.Direction == EPinDirection::Output)
				{
					for (const FString& TargetNodeId : Pin.LinkedNodeIds)
					{
						AddDirectedEdge(Node.NodeId, TargetNodeId);
					}
				}
				else if (Pin.Direction == EPinDirection::Input && !Pin.bExec)
				{
					for (const FString& SourceNodeId : Pin.LinkedNodeIds)
					{
						AddDirectedEdge(SourceNodeId, Node.NodeId);
					}
				}
			}
		}
		return Adjacency;
	}

	static TArray<FGraphLayoutLayeredComponent> FindComponents(
		const TMap<FString, TSet<FString>>& Undirected,
		const FGraphLayoutArrangeScope& Scope,
		const TArray<FNodeSnapshot>& SnapshotNodes)
	{
		TSet<FString> Visited;
		TArray<FGraphLayoutLayeredComponent> Components;

		for (const FNodeSnapshot& SnapshotNode : SnapshotNodes)
		{
			if (Visited.Contains(SnapshotNode.NodeId))
			{
				continue;
			}

			FGraphLayoutLayeredComponent Component;
			TArray<FString> Queue;
			Queue.Add(SnapshotNode.NodeId);
			for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
			{
				const FString NodeId = Queue[QueueIndex];
				if (Visited.Contains(NodeId))
				{
					continue;
				}
				Visited.Add(NodeId);
				Component.NodeIds.Add(NodeId);
				if (Scope.ToArrangeNodeIds.Contains(NodeId))
				{
					Component.ArrangeNodeIds.Add(NodeId);
				}
				else
				{
					Component.AnchorNodeIds.Add(NodeId);
				}

				if (const TSet<FString>* Neighbours = Undirected.Find(NodeId))
				{
					for (const FString& Neighbour : *Neighbours)
					{
						if (!Visited.Contains(Neighbour))
						{
							Queue.Add(Neighbour);
						}
					}
				}
			}

			if (Component.ArrangeNodeIds.Num() == 0)
			{
				Component.Kind = EGraphLayoutComponentKind::FixedOnly;
			}
			else if (Component.AnchorNodeIds.Num() == 0)
			{
				Component.Kind = EGraphLayoutComponentKind::FullArrange;
			}
			else
			{
				Component.Kind = EGraphLayoutComponentKind::PartialAnchored;
			}
			Components.Add(Component);
		}

		return Components;
	}

	static TMap<FString, int32> AssignDepths(
		const FGraphLayoutLayeredComponent& Component,
		const FGraphLayoutLayeredAdjacency& Adjacency)
	{
		TMap<FString, int32> DepthByNodeId;
		TMap<FString, int32> RemainingIncoming;
		for (const FString& NodeId : Component.NodeIds)
		{
			DepthByNodeId.Add(NodeId, 0);
			int32 IncomingCount = 0;
			if (const TSet<FString>* Upstream = Adjacency.Upstream.Find(NodeId))
			{
				for (const FString& SourceNodeId : *Upstream)
				{
					if (Component.NodeIds.Contains(SourceNodeId))
					{
						++IncomingCount;
					}
				}
			}
			RemainingIncoming.Add(NodeId, IncomingCount);
		}

		TArray<FString> Queue;
		for (const TPair<FString, int32>& Pair : RemainingIncoming)
		{
			if (Pair.Value == 0)
			{
				Queue.Add(Pair.Key);
			}
		}
		Queue.Sort();

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const FString NodeId = Queue[QueueIndex];
			const int32 NodeDepth = DepthByNodeId.FindRef(NodeId);
			const TSet<FString>* Downstream = Adjacency.Downstream.Find(NodeId);
			if (!Downstream)
			{
				continue;
			}
			TArray<FString> SortedDownstream = Downstream->Array();
			SortedDownstream.Sort();
			for (const FString& TargetNodeId : SortedDownstream)
			{
				if (!Component.NodeIds.Contains(TargetNodeId))
				{
					continue;
				}
				DepthByNodeId.FindOrAdd(TargetNodeId) =
					FMath::Max(DepthByNodeId.FindRef(TargetNodeId), NodeDepth + 1);
				int32& Remaining = RemainingIncoming.FindOrAdd(TargetNodeId);
				--Remaining;
				if (Remaining == 0)
				{
					Queue.Add(TargetNodeId);
				}
			}
		}

		for (const FString& NodeId : Component.NodeIds)
		{
			const TSet<FString>* Upstream = Adjacency.Upstream.Find(NodeId);
			const TSet<FString>* Downstream = Adjacency.Downstream.Find(NodeId);
			bool bHasIncomingInComponent = false;
			if (Upstream)
			{
				for (const FString& SourceNodeId : *Upstream)
				{
					if (Component.NodeIds.Contains(SourceNodeId))
					{
						bHasIncomingInComponent = true;
						break;
					}
				}
			}
			if (bHasIncomingInComponent || !Downstream)
			{
				continue;
			}

			int32 MinConsumerDepth = MAX_int32;
			for (const FString& TargetNodeId : *Downstream)
			{
				if (Component.NodeIds.Contains(TargetNodeId))
				{
					MinConsumerDepth = FMath::Min(MinConsumerDepth, DepthByNodeId.FindRef(TargetNodeId));
				}
			}
			if (MinConsumerDepth != MAX_int32)
			{
				DepthByNodeId.FindOrAdd(NodeId) = FMath::Max(0, MinConsumerDepth - 1);
			}
		}

		return DepthByNodeId;
	}

	static TMap<int32, float> ComputeColumnXOffsets(
		const FGraphLayoutLayeredComponent& Component,
		const TMap<FString, int32>& DepthByNodeId,
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		const FRuleSet& RuleSet)
	{
		TArray<int32> Columns;
		TMap<int32, float> ColumnWidths;
		for (const FString& NodeId : Component.NodeIds)
		{
			const int32 Depth = DepthByNodeId.FindRef(NodeId);
			Columns.AddUnique(Depth);
			const FNodeSnapshot* Node = FindSnapshotNode(NodesById, NodeId);
			const float Width = Node ? FMath::Max(1.0f, Node->Size.X) : 180.0f;
			ColumnWidths.FindOrAdd(Depth) = FMath::Max(
				ColumnWidths.FindRef(Depth),
				Width + FMath::Max(1.0f, RuleSet.ExecColumnSpacing));
		}
		Columns.Sort();

		TMap<int32, float> Offsets;
		float X = 0.0f;
		for (const int32 Column : Columns)
		{
			Offsets.Add(Column, X);
			X += FMath::Max(1.0f, ColumnWidths.FindRef(Column));
		}
		return Offsets;
	}

	static TMap<FString, float> AssignYCoordinates(
		const FGraphLayoutLayeredComponent& Component,
		const TMap<FString, int32>& DepthByNodeId,
		const FGraphLayoutLayeredAdjacency& Adjacency,
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		const FRuleSet& RuleSet)
	{
		TMap<int32, TArray<FString>> NodesByColumn;
		for (const FString& NodeId : Component.NodeIds)
		{
			NodesByColumn.FindOrAdd(DepthByNodeId.FindRef(NodeId)).Add(NodeId);
		}

		TArray<int32> Columns;
		NodesByColumn.GetKeys(Columns);
		Columns.Sort();

		float BaseY = 0.0f;
		bool bHasBaseY = false;
		for (const FString& AnchorNodeId : Component.AnchorNodeIds)
		{
			if (const FNodeSnapshot* Node = FindSnapshotNode(NodesById, AnchorNodeId))
			{
				BaseY = bHasBaseY ? FMath::Min(BaseY, Node->Position.Y) : Node->Position.Y;
				bHasBaseY = true;
			}
		}
		for (const FString& ArrangeNodeId : Component.ArrangeNodeIds)
		{
			if (const FNodeSnapshot* Node = FindSnapshotNode(NodesById, ArrangeNodeId))
			{
				BaseY = bHasBaseY ? FMath::Min(BaseY, Node->Position.Y) : Node->Position.Y;
				bHasBaseY = true;
			}
		}

		TMap<FString, float> YByNodeId;
		for (const int32 Column : Columns)
		{
			TArray<FString>& ColumnNodeIds = NodesByColumn.FindChecked(Column);
			ColumnNodeIds.Sort([&NodesById](const FString& Left, const FString& Right)
			{
				const FNodeSnapshot* LeftNode = FindSnapshotNode(NodesById, Left);
				const FNodeSnapshot* RightNode = FindSnapshotNode(NodesById, Right);
				const float LeftY = LeftNode ? LeftNode->Position.Y : 0.0f;
				const float RightY = RightNode ? RightNode->Position.Y : 0.0f;
				if (!FMath::IsNearlyEqual(LeftY, RightY))
				{
					return LeftY < RightY;
				}
				return Left < Right;
			});

			float NextY = BaseY;
			TArray<float> FixedYs;
			for (const FString& NodeId : ColumnNodeIds)
			{
				if (Component.ArrangeNodeIds.Contains(NodeId))
				{
					continue;
				}
				if (const FNodeSnapshot* Node = FindSnapshotNode(NodesById, NodeId))
				{
					YByNodeId.Add(NodeId, Node->Position.Y);
					FixedYs.Add(Node->Position.Y);
				}
			}

			for (const FString& NodeId : ColumnNodeIds)
			{
				if (!Component.ArrangeNodeIds.Contains(NodeId))
				{
					continue;
				}

				const FNodeSnapshot* Node = FindSnapshotNode(NodesById, NodeId);
				const float Height = Node ? FMath::Max(1.0f, Node->Size.Y) : 80.0f;
				const float RowSpacing = Height + FMath::Max(1.0f, RuleSet.ExecRowSpacing);
				float PreferredY = NextY;
				if (const TSet<FString>* Upstream = Adjacency.Upstream.Find(NodeId))
				{
					float SumY = 0.0f;
					int32 Count = 0;
					for (const FString& SourceNodeId : *Upstream)
					{
						if (const float* UpstreamY = YByNodeId.Find(SourceNodeId))
						{
							SumY += *UpstreamY;
							++Count;
						}
					}
					if (Count > 0)
					{
						PreferredY = SumY / Count;
					}
				}

				float Y = FMath::Max(NextY, PreferredY);
				for (bool bAdjusted = true; bAdjusted;)
				{
					bAdjusted = false;
					for (const float FixedY : FixedYs)
					{
						if (FMath::Abs(Y - FixedY) < RowSpacing)
						{
							Y += RowSpacing;
							bAdjusted = true;
						}
					}
				}
				YByNodeId.Add(NodeId, Y);
				NextY = Y + RowSpacing;
			}
		}

		return YByNodeId;
	}

	static void TranslateToCanvas(
		const FGraphLayoutLayeredComponent& Component,
		const TMap<FString, int32>& DepthByNodeId,
		const TMap<int32, float>& ColumnXOffsets,
		const TMap<FString, float>& YByNodeId,
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		TMap<FString, FVector2D>& InOutPositions)
	{
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;
		if (Component.AnchorNodeIds.Num() > 0)
		{
			float SumOffsetX = 0.0f;
			int32 Count = 0;
			for (const FString& AnchorNodeId : Component.AnchorNodeIds)
			{
				const FNodeSnapshot* Node = FindSnapshotNode(NodesById, AnchorNodeId);
				if (!Node)
				{
					continue;
				}
				SumOffsetX += Node->Position.X - ColumnXOffsets.FindRef(DepthByNodeId.FindRef(AnchorNodeId));
				++Count;
			}
			OffsetX = Count > 0 ? SumOffsetX / Count : 0.0f;
		}
		else
		{
			float OriginalMinX = 0.0f;
			float OriginalMinY = 0.0f;
			float ResultMinX = 0.0f;
			float ResultMinY = 0.0f;
			bool bInitialized = false;
			for (const FString& ArrangeNodeId : Component.ArrangeNodeIds)
			{
				const FNodeSnapshot* Node = FindSnapshotNode(NodesById, ArrangeNodeId);
				if (!Node)
				{
					continue;
				}
				const float ResultX = ColumnXOffsets.FindRef(DepthByNodeId.FindRef(ArrangeNodeId));
				const float ResultY = YByNodeId.FindRef(ArrangeNodeId);
				OriginalMinX = bInitialized ? FMath::Min(OriginalMinX, Node->Position.X) : Node->Position.X;
				OriginalMinY = bInitialized ? FMath::Min(OriginalMinY, Node->Position.Y) : Node->Position.Y;
				ResultMinX = bInitialized ? FMath::Min(ResultMinX, ResultX) : ResultX;
				ResultMinY = bInitialized ? FMath::Min(ResultMinY, ResultY) : ResultY;
				bInitialized = true;
			}
			OffsetX = OriginalMinX - ResultMinX;
			OffsetY = OriginalMinY - ResultMinY;
		}

		for (const FString& ArrangeNodeId : Component.ArrangeNodeIds)
		{
			InOutPositions.Add(
				ArrangeNodeId,
				FVector2D(
					ColumnXOffsets.FindRef(DepthByNodeId.FindRef(ArrangeNodeId)) + OffsetX,
					YByNodeId.FindRef(ArrangeNodeId) + OffsetY));
		}
	}

	static FGraphLayoutLayeredBounds BuildBounds(
		const TSet<FString>& NodeIds,
		const TMap<FString, FVector2D>& Positions,
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		const FVector2D& Padding)
	{
		FGraphLayoutLayeredBounds Bounds;
		for (const FString& NodeId : NodeIds)
		{
			const FNodeSnapshot* Node = FindSnapshotNode(NodesById, NodeId);
			if (!Node)
			{
				continue;
			}
			const FVector2D Position = Positions.FindRef(NodeId);
			const FVector2D Min = Position - Padding;
			const FVector2D Max = Position + Node->Size + Padding;
			if (!Bounds.bValid)
			{
				Bounds.Min = Min;
				Bounds.Max = Max;
				Bounds.bValid = true;
			}
			else
			{
				Bounds.Min.X = FMath::Min(Bounds.Min.X, Min.X);
				Bounds.Min.Y = FMath::Min(Bounds.Min.Y, Min.Y);
				Bounds.Max.X = FMath::Max(Bounds.Max.X, Max.X);
				Bounds.Max.Y = FMath::Max(Bounds.Max.Y, Max.Y);
			}
		}
		return Bounds;
	}

	static void PackFullComponents(
		const TArray<FGraphLayoutLayeredComponent>& Components,
		const TMap<FString, const FNodeSnapshot*>& NodesById,
		const FRuleSet& RuleSet,
		TMap<FString, FVector2D>& InOutPositions)
	{
		const FVector2D Padding(
			FMath::Max(0.0f, RuleSet.CollisionPaddingX),
			FMath::Max(0.0f, RuleSet.CollisionPaddingY));
		const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);
		TArray<FGraphLayoutLayeredBounds> PlacedBounds;

		TArray<FGraphLayoutLayeredComponent> SortedComponents = Components;
		SortedComponents.Sort([&NodesById, &InOutPositions, &Padding](const FGraphLayoutLayeredComponent& Left, const FGraphLayoutLayeredComponent& Right)
		{
			const FGraphLayoutLayeredBounds LeftBounds = BuildBounds(Left.NodeIds, InOutPositions, NodesById, Padding);
			const FGraphLayoutLayeredBounds RightBounds = BuildBounds(Right.NodeIds, InOutPositions, NodesById, Padding);
			if (!FMath::IsNearlyEqual(LeftBounds.Min.Y, RightBounds.Min.Y))
			{
				return LeftBounds.Min.Y < RightBounds.Min.Y;
			}
			return LeftBounds.Min.X < RightBounds.Min.X;
		});

		for (const FGraphLayoutLayeredComponent& Component : SortedComponents)
		{
			if (Component.Kind != EGraphLayoutComponentKind::FullArrange)
			{
				PlacedBounds.Add(BuildBounds(Component.NodeIds, InOutPositions, NodesById, Padding));
				continue;
			}

			FGraphLayoutLayeredBounds Bounds = BuildBounds(Component.NodeIds, InOutPositions, NodesById, Padding);
			FVector2D Offset = FVector2D::ZeroVector;
			for (bool bAdjusted = true; bAdjusted;)
			{
				bAdjusted = false;
				for (const FGraphLayoutLayeredBounds& Placed : PlacedBounds)
				{
					if (!Bounds.Overlaps(Placed))
					{
						continue;
					}
					const float DeltaY = (Placed.Max.Y - Bounds.Min.Y) + StepY;
					Offset.Y += FMath::Max(StepY, DeltaY);
					Bounds.Offset(FVector2D(0.0f, FMath::Max(StepY, DeltaY)));
					bAdjusted = true;
				}
			}

			if (!Offset.IsNearlyZero())
			{
				for (const FString& NodeId : Component.ArrangeNodeIds)
				{
					if (FVector2D* Position = InOutPositions.Find(NodeId))
					{
						*Position += Offset;
					}
				}
			}
			PlacedBounds.Add(Bounds);
		}
	}
};

FGraphLayoutLayeredResult FGraphLayoutLayeredComponentPolicy::Layout(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const FGraphLayoutArrangeScope& Scope,
	const FRuleSet& RuleSet)
{
	(void)Topology;

	FGraphLayoutLayeredResult Result;
	TMap<FString, const FNodeSnapshot*> NodesById;
	TMap<FString, FVector2D> Positions;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		NodesById.Add(Node.NodeId, &Node);
		Positions.Add(Node.NodeId, Node.Position);
	}

	const FGraphLayoutLayeredAdjacency Adjacency =
		FGraphLayoutLayeredComponentPolicyPrivate::BuildAdjacency(Snapshot);
	const TArray<FGraphLayoutLayeredComponent> Components =
		FGraphLayoutLayeredComponentPolicyPrivate::FindComponents(Adjacency.Undirected, Scope, Snapshot.Nodes);

	for (const FGraphLayoutLayeredComponent& Component : Components)
	{
		if (Component.ArrangeNodeIds.Num() == 0)
		{
			continue;
		}

		const TMap<FString, int32> DepthByNodeId =
			FGraphLayoutLayeredComponentPolicyPrivate::AssignDepths(Component, Adjacency);
		const TMap<int32, float> ColumnXOffsets =
			FGraphLayoutLayeredComponentPolicyPrivate::ComputeColumnXOffsets(Component, DepthByNodeId, NodesById, RuleSet);
		const TMap<FString, float> YByNodeId =
			FGraphLayoutLayeredComponentPolicyPrivate::AssignYCoordinates(Component, DepthByNodeId, Adjacency, NodesById, RuleSet);
		FGraphLayoutLayeredComponentPolicyPrivate::TranslateToCanvas(
			Component,
			DepthByNodeId,
			ColumnXOffsets,
			YByNodeId,
			NodesById,
			Positions);
	}

	FGraphLayoutLayeredComponentPolicyPrivate::PackFullComponents(Components, NodesById, RuleSet, Positions);

	for (const FString& NodeId : Scope.ToArrangeNodeIds)
	{
		FGraphLayoutLayeredPlacement& Placement = Result.Placements.AddDefaulted_GetRef();
		Placement.NodeId = NodeId;
		Placement.TargetPosition = Positions.FindRef(NodeId);
		Placement.Reason = TEXT("layered_component_layout");
	}
	return Result;
}
}
