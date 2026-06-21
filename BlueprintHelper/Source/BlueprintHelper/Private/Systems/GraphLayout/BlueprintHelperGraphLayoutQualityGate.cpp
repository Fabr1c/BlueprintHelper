#include "Systems/GraphLayout/BlueprintHelperGraphLayoutQualityGate.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutQualityNode
{
	FString NodeId;
	FString LayoutGroupId;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(180.0f, 80.0f);
	bool bExisting = true;
	bool bMovedByPlan = false;
};

struct FGraphLayoutQualityEdge
{
	FString SourceNodeId;
	FString TargetNodeId;
	FString PinId;
	bool bExec = false;
};

struct FGraphLayoutQualityGatePrivate
{
	static FVector2D GetCenter(const FGraphLayoutQualityNode& Node)
	{
		return Node.Position + Node.Size * 0.5f;
	}

	static float ComputeDistance(const FGraphLayoutQualityNode& A, const FGraphLayoutQualityNode& B)
	{
		return FVector2D::Distance(GetCenter(A), GetCenter(B));
	}

	static float ComputeOverlapRatio(
		const FGraphLayoutQualityNode& A,
		const FGraphLayoutQualityNode& B)
	{
		const float Left = FMath::Max(A.Position.X, B.Position.X);
		const float Right = FMath::Min(A.Position.X + A.Size.X, B.Position.X + B.Size.X);
		const float Top = FMath::Max(A.Position.Y, B.Position.Y);
		const float Bottom = FMath::Min(A.Position.Y + A.Size.Y, B.Position.Y + B.Size.Y);
		if (Right <= Left || Bottom <= Top)
		{
			return 0.0f;
		}

		const float OverlapArea = (Right - Left) * (Bottom - Top);
		const float MinArea = FMath::Max(1.0f, FMath::Min(
			FMath::Max(1.0f, A.Size.X) * FMath::Max(1.0f, A.Size.Y),
			FMath::Max(1.0f, B.Size.X) * FMath::Max(1.0f, B.Size.Y)));
		return OverlapArea / MinArea;
	}

	static bool IsSameExplicitGroup(
		const FGraphLayoutQualityNode& A,
		const FGraphLayoutQualityNode& B)
	{
		return !A.LayoutGroupId.IsEmpty() && A.LayoutGroupId == B.LayoutGroupId;
	}

	static TArray<FGraphLayoutQualityEdge> BuildDirectedEdges(const FGraphSnapshot& Snapshot)
	{
		TArray<FGraphLayoutQualityEdge> Edges;
		for (const FNodeSnapshot& Node : Snapshot.Nodes)
		{
			for (const FPinSnapshot& Pin : Node.Pins)
			{
				if (Pin.Direction != EPinDirection::Output)
				{
					continue;
				}

				for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
				{
					FGraphLayoutQualityEdge& Edge = Edges.AddDefaulted_GetRef();
					Edge.SourceNodeId = Node.NodeId;
					Edge.TargetNodeId = LinkedNodeId;
					Edge.PinId = Pin.PinId;
					Edge.bExec = Pin.bExec;
				}
			}
		}
		return Edges;
	}

	static float ComputeMaxReasonableEdgeLength(const FRuleSet& RuleSet)
	{
		const float BaseSpacing = FMath::Max3(
			FMath::Max(1.0f, RuleSet.ExecColumnSpacing),
			FMath::Max(1.0f, RuleSet.PureInputOffsetX),
			FMath::Max(1.0f, RuleSet.VariableInputOffsetX));
		return BaseSpacing * 6.0f + FMath::Max(0.0f, RuleSet.CollisionPaddingX) * 2.0f;
	}

	static void AddIssue(
		FGraphLayoutQualityResult& Result,
		const FString& Code,
		const FString& NodeA,
		const FString& NodeB,
		const FString& Message)
	{
		FGraphLayoutQualityIssue& Issue = Result.Issues.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.NodeA = NodeA;
		Issue.NodeB = NodeB;
		Issue.Message = Message;
	}
};

FGraphLayoutQualityResult FGraphLayoutQualityGate::Evaluate(
	const FGraphSnapshot& Snapshot,
	const FLayoutPlan& Plan,
	const FRuleSet& RuleSet)
{
	TMap<FString, const FNodePlacement*> PlacementsByNodeId;
	for (const FNodePlacement& Placement : Plan.Placements)
	{
		PlacementsByNodeId.Add(Placement.NodeId, &Placement);
	}

	TArray<FGraphLayoutQualityNode> Nodes;
	Nodes.Reserve(Snapshot.Nodes.Num());
	TMap<FString, const FGraphLayoutQualityNode*> NodesById;
	for (const FNodeSnapshot& SnapshotNode : Snapshot.Nodes)
	{
		const FNodePlacement* Placement = PlacementsByNodeId.FindRef(SnapshotNode.NodeId);
		FGraphLayoutQualityNode& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = SnapshotNode.NodeId;
		Node.LayoutGroupId = SnapshotNode.LayoutBlockId;
		Node.Position = Placement ? Placement->TargetPosition : SnapshotNode.Position;
		Node.Size = SnapshotNode.Size;
		Node.bExisting = SnapshotNode.bExisting;
		Node.bMovedByPlan = Placement && Placement->bMoveExisting;
	}
	for (const FGraphLayoutQualityNode& Node : Nodes)
	{
		NodesById.Add(Node.NodeId, &Node);
	}

	FGraphLayoutQualityResult Result;
	const float Tolerance = FMath::Clamp(RuleSet.OverlapToleranceRatio, 0.0f, 0.5f);
	for (int32 LeftIndex = 0; LeftIndex < Nodes.Num(); ++LeftIndex)
	{
		for (int32 RightIndex = LeftIndex + 1; RightIndex < Nodes.Num(); ++RightIndex)
		{
			const FGraphLayoutQualityNode& Left = Nodes[LeftIndex];
			const FGraphLayoutQualityNode& Right = Nodes[RightIndex];
			if (FGraphLayoutQualityGatePrivate::IsSameExplicitGroup(Left, Right))
			{
				continue;
			}

			const float OverlapRatio = FGraphLayoutQualityGatePrivate::ComputeOverlapRatio(Left, Right);
			if (OverlapRatio <= Tolerance)
			{
				continue;
			}

			const bool bExistingIntrusion =
				(Left.bExisting && Right.bMovedByPlan) ||
				(Right.bExisting && Left.bMovedByPlan);
			FGraphLayoutQualityGatePrivate::AddIssue(
				Result,
				bExistingIntrusion ? TEXT("existing_node_intrusion") : TEXT("overlap_beyond_tolerance"),
				Left.NodeId,
				Right.NodeId,
				FString::Printf(
				TEXT("%s overlaps %s by %.3f beyond tolerance %.3f"),
				*Left.NodeId,
				*Right.NodeId,
				OverlapRatio,
					Tolerance));
		}
	}

	const float MaxReasonableEdgeLength =
		FGraphLayoutQualityGatePrivate::ComputeMaxReasonableEdgeLength(RuleSet);
	for (const FGraphLayoutQualityEdge& Edge : FGraphLayoutQualityGatePrivate::BuildDirectedEdges(Snapshot))
	{
		const FGraphLayoutQualityNode* Source = NodesById.FindRef(Edge.SourceNodeId);
		const FGraphLayoutQualityNode* Target = NodesById.FindRef(Edge.TargetNodeId);
		if (!Source || !Target)
		{
			continue;
		}

		const FVector2D SourceCenter = FGraphLayoutQualityGatePrivate::GetCenter(*Source);
		const FVector2D TargetCenter = FGraphLayoutQualityGatePrivate::GetCenter(*Target);
		if (TargetCenter.X + KINDA_SMALL_NUMBER < SourceCenter.X)
		{
			FGraphLayoutQualityGatePrivate::AddIssue(
				Result,
				TEXT("reverse_edge"),
				Edge.SourceNodeId,
				Edge.TargetNodeId,
				FString::Printf(
					TEXT("%s -> %s points backwards on X axis"),
					*Edge.SourceNodeId,
					*Edge.TargetNodeId));
		}

		const float EdgeLength = FGraphLayoutQualityGatePrivate::ComputeDistance(*Source, *Target);
		if (EdgeLength > MaxReasonableEdgeLength)
		{
			FGraphLayoutQualityGatePrivate::AddIssue(
				Result,
				TEXT("overlong_edge"),
				Edge.SourceNodeId,
				Edge.TargetNodeId,
				FString::Printf(
					TEXT("%s -> %s length %.1f exceeds reasonable threshold %.1f"),
					*Edge.SourceNodeId,
					*Edge.TargetNodeId,
					EdgeLength,
					MaxReasonableEdgeLength));
		}
	}
	return Result;
}
}
