#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewOverlayProjector.h"

namespace BlueprintHelper::GraphLayout
{
const FGraphLayoutPreviewNodeSpec* FGraphLayoutPreviewOverlayProjector::FindNodeSpec(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Nodes.FindByPredicate([&NodeId](const FGraphLayoutPreviewNodeSpec& Candidate)
	{
		return Candidate.NodeId == NodeId;
	});
}

const FNodeSnapshot* FGraphLayoutPreviewOverlayProjector::FindSnapshotNode(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Snapshot.Nodes.FindByPredicate([&NodeId](const FNodeSnapshot& Candidate)
	{
		return Candidate.NodeId == NodeId;
	});
}

const FNodePlacement* FGraphLayoutPreviewOverlayProjector::FindPlacement(
	const FLayoutPlan& Plan,
	const FString& NodeId)
{
	return Plan.Placements.FindByPredicate([&NodeId](const FNodePlacement& Candidate)
	{
		return Candidate.NodeId == NodeId;
	});
}

bool FGraphLayoutPreviewOverlayProjector::HasPlacementForNode(
	const FLayoutPlan& Plan,
	const FString& NodeId)
{
	return FindPlacement(Plan, NodeId) != nullptr;
}

void FGraphLayoutPreviewOverlayProjector::AddPlacement(
	FLayoutPlan& Plan,
	const FGraphLayoutPreviewSample& Sample,
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const FVector2D& TargetPosition,
	const FString& Reason,
	const FVector2D& TargetSize)
{
	if (HasPlacementForNode(Plan, NodeSpec.NodeId))
	{
		return;
	}

	const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
	FNodePlacement Placement;
	Placement.NodeId = NodeSpec.NodeId;
	Placement.Role = NodeSpec.Role;
	Placement.CurrentPosition = SnapshotNode ? SnapshotNode->Position : FVector2D::ZeroVector;
	Placement.TargetPosition = TargetPosition;
	Placement.TargetSize = TargetSize;
	Placement.bMoveExisting = true;
	Placement.Reason = Reason;
	Plan.Placements.Add(Placement);
}

void FGraphLayoutPreviewOverlayProjector::AppendEntryAvoidanceRangeComments(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	FLayoutPlan& Plan)
{
	const FGraphLayoutPreviewNodeSpec* EntrySpec = FindNodeSpec(Sample, TEXT("EventStart"));
	const FGraphLayoutPreviewNodeSpec* HorizontalSpec = FindNodeSpec(Sample, TEXT("HorizontalAvoidanceRange"));
	const FGraphLayoutPreviewNodeSpec* VerticalSpec = FindNodeSpec(Sample, TEXT("VerticalAvoidanceRange"));
	const FNodePlacement* EntryPlacement = EntrySpec ? FindPlacement(Plan, EntrySpec->NodeId) : nullptr;
	if (!EntrySpec || !HorizontalSpec || !VerticalSpec || !EntryPlacement)
	{
		return;
	}

	const float HorizontalWidth =
		EntrySpec->Size.X + RuleSet.CollisionPaddingX * 2.0f +
		RuleSet.MaxCollisionAttempts * FMath::Max(EntrySpec->Size.X, RuleSet.CollisionPaddingX);
	const float HorizontalHeight = EntrySpec->Size.Y + RuleSet.CollisionPaddingY * 2.0f;
	const float VerticalWidth = EntrySpec->Size.X + RuleSet.CollisionPaddingX * 2.0f;
	const float VerticalHeight =
		EntrySpec->Size.Y + RuleSet.CollisionPaddingY * 2.0f +
		RuleSet.MaxCollisionAttempts * RuleSet.CollisionStepY;

	AddPlacement(
		Plan,
		Sample,
		*HorizontalSpec,
		FVector2D(
			EntryPlacement->TargetPosition.X - RuleSet.CollisionPaddingX,
			EntryPlacement->TargetPosition.Y - RuleSet.CollisionPaddingY - HorizontalHeight - 24.0f),
		TEXT("preview_horizontal_avoidance_range"),
		FVector2D(HorizontalWidth, HorizontalHeight));
	AddPlacement(
		Plan,
		Sample,
		*VerticalSpec,
		FVector2D(
			EntryPlacement->TargetPosition.X - RuleSet.CollisionPaddingX,
			EntryPlacement->TargetPosition.Y + EntrySpec->Size.Y + 24.0f),
		TEXT("preview_vertical_avoidance_range"),
		FVector2D(VerticalWidth, VerticalHeight));
}

void FGraphLayoutPreviewOverlayProjector::AppendSemanticLabelComments(
	const FGraphLayoutPreviewSample& Sample,
	FLayoutPlan& Plan)
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (!NodeSpec.bPreviewSemanticLabel || NodeSpec.PreviewLabelTargetNodeId.IsEmpty())
		{
			continue;
		}

		const FNodePlacement* TargetPlacement = FindPlacement(Plan, NodeSpec.PreviewLabelTargetNodeId);
		if (!TargetPlacement)
		{
			continue;
		}

		AddPlacement(
			Plan,
			Sample,
			NodeSpec,
			TargetPlacement->TargetPosition + NodeSpec.PreviewLabelOffset,
			TEXT("preview_semantic_label"),
			NodeSpec.Size);
	}
}

void FGraphLayoutPreviewOverlayProjector::AppendOverlays(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	FLayoutPlan& Plan)
{
	AppendEntryAvoidanceRangeComments(Sample, RuleSet, Plan);
	AppendSemanticLabelComments(Sample, Plan);
}
}
