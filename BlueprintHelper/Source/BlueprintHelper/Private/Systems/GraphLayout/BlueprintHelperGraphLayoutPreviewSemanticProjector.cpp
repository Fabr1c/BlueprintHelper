#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutExecPinAnchor.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"

namespace BlueprintHelper::GraphLayout
{
FVector2D FGraphLayoutPreviewSemanticProjector::GetAnchorOffset(
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const ENodeRole AnchorRole)
{
	const FVector2D Size = NodeSpec.Size;
	return FGraphLayoutExecPinAnchor::GetPrimaryExecAnchorOffset(Size, AnchorRole);
}

FVector2D FGraphLayoutPreviewSemanticProjector::BuildTopLeftFromAnchor(
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const ENodeRole AnchorRole,
	const FVector2D& AnchorPosition)
{
	return AnchorPosition - GetAnchorOffset(NodeSpec, AnchorRole);
}

const FGraphLayoutPreviewNodeSpec* FGraphLayoutPreviewSemanticProjector::FindNodeSpec(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Nodes.FindByPredicate([&NodeId](const FGraphLayoutPreviewNodeSpec& NodeSpec)
	{
		return NodeSpec.NodeId == NodeId;
	});
}

const FNodeSnapshot* FGraphLayoutPreviewSemanticProjector::FindSnapshotNode(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Snapshot.Nodes.FindByPredicate([&NodeId](const FNodeSnapshot& Node)
	{
		return Node.NodeId == NodeId;
	});
}

const FNodePlacement* FGraphLayoutPreviewSemanticProjector::FindPlacement(
	const FLayoutPlan& Plan,
	const FString& NodeId)
{
	return Plan.Placements.FindByPredicate([&NodeId](const FNodePlacement& Placement)
	{
		return Placement.NodeId == NodeId;
	});
}

bool FGraphLayoutPreviewSemanticProjector::HasPlacementForNode(
	const FLayoutPlan& Plan,
	const FString& NodeId)
{
	return FindPlacement(Plan, NodeId) != nullptr;
}

void FGraphLayoutPreviewSemanticProjector::AddPlacement(
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

void FGraphLayoutPreviewSemanticProjector::ProjectEntryAvoidanceRangeComments(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	FLayoutPlan& Plan)
{
	const FGraphLayoutPreviewNodeSpec* EntrySpec = FindNodeSpec(Sample, TEXT("EventStart"));
	const FGraphLayoutPreviewNodeSpec* HorizontalSpec = FindNodeSpec(Sample, TEXT("HorizontalAvoidanceRange"));
	const FGraphLayoutPreviewNodeSpec* VerticalSpec = FindNodeSpec(Sample, TEXT("VerticalAvoidanceRange"));
	if (!EntrySpec || !HorizontalSpec || !VerticalSpec)
	{
		return;
	}

	FVector2D EntryTarget = FVector2D::ZeroVector;
	if (!TryBuildSampleRelativeTarget(Sample, Plan, EntrySpec->NodeId, EntryTarget))
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

	const FVector2D HorizontalSize(HorizontalWidth, HorizontalHeight);
	const FVector2D VerticalSize(VerticalWidth, VerticalHeight);
	const FVector2D HorizontalTarget(
		EntryTarget.X - RuleSet.CollisionPaddingX,
		EntryTarget.Y - RuleSet.CollisionPaddingY - HorizontalHeight - 24.0f);
	const FVector2D VerticalTarget(
		EntryTarget.X - RuleSet.CollisionPaddingX,
		EntryTarget.Y + EntrySpec->Size.Y + 24.0f);

	AddPlacement(
		Plan,
		Sample,
		*HorizontalSpec,
		HorizontalTarget,
		TEXT("preview_horizontal_avoidance_range"),
		HorizontalSize);
	AddPlacement(
		Plan,
		Sample,
		*VerticalSpec,
		VerticalTarget,
		TEXT("preview_vertical_avoidance_range"),
		VerticalSize);
}

void FGraphLayoutPreviewSemanticProjector::ProjectSemanticLabelComments(
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

void FGraphLayoutPreviewSemanticProjector::ProjectAnchoredNodesByRole(
	const FGraphLayoutPreviewSample& Sample,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (!NodeSpec.bUsePreviewRoleAnchor || HasPlacementForNode(Plan, NodeSpec.NodeId))
		{
			continue;
		}

		const FVector2D* Anchor = SceneState.RoleCenters.Find(NodeSpec.PreviewAnchorRole);
		if (!Anchor)
		{
			Plan.Issues.Add(FString::Printf(
				TEXT("preview semantic scene is missing role center for node \"%s\""),
				*NodeSpec.NodeId));
			continue;
		}

		AddPlacement(
			Plan,
			Sample,
			NodeSpec,
			BuildTopLeftFromAnchor(NodeSpec, NodeSpec.PreviewAnchorRole, *Anchor),
			TEXT("preview_semantic_role_anchor"));
	}
}

void FGraphLayoutPreviewSemanticProjector::ProjectRemainingNodesBySampleOffset(
	const FGraphLayoutPreviewSample& Sample,
	FLayoutPlan& Plan)
{
	FVector2D Translation = FVector2D::ZeroVector;
	bool bHasTranslation = false;

	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		const FNodePlacement* Placement = FindPlacement(Plan, NodeSpec.NodeId);
		const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
		if (Placement && SnapshotNode)
		{
			Translation = Placement->TargetPosition - SnapshotNode->Position;
			bHasTranslation = true;
			break;
		}
	}

	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (HasPlacementForNode(Plan, NodeSpec.NodeId))
		{
			continue;
		}

		const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
		const FVector2D SamplePosition = SnapshotNode ? SnapshotNode->Position : FVector2D::ZeroVector;
		AddPlacement(
			Plan,
			Sample,
			NodeSpec,
			bHasTranslation ? SamplePosition + Translation : SamplePosition,
			TEXT("preview_sample_relative"));
	}
}

bool FGraphLayoutPreviewSemanticProjector::TryBuildSampleRelativeTarget(
	const FGraphLayoutPreviewSample& Sample,
	const FLayoutPlan& Plan,
	const FString& NodeId,
	FVector2D& OutTargetPosition)
{
	if (const FNodePlacement* ExistingPlacement = FindPlacement(Plan, NodeId))
	{
		OutTargetPosition = ExistingPlacement->TargetPosition;
		return true;
	}

	const FNodeSnapshot* TargetSnapshot = FindSnapshotNode(Sample, NodeId);
	if (!TargetSnapshot)
	{
		return false;
	}

	FVector2D Translation = FVector2D::ZeroVector;
	bool bHasTranslation = false;
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		const FNodePlacement* Placement = FindPlacement(Plan, NodeSpec.NodeId);
		const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
		if (Placement && SnapshotNode)
		{
			Translation = Placement->TargetPosition - SnapshotNode->Position;
			bHasTranslation = true;
			break;
		}
	}

	OutTargetPosition = bHasTranslation
		? TargetSnapshot->Position + Translation
		: TargetSnapshot->Position;
	return true;
}

void FGraphLayoutPreviewSemanticProjector::ProjectExecContextLink(
	const FGraphLayoutPreviewSample& Sample,
	FLayoutPlan& Plan,
	const FString& EventNodeId,
	const FString& ConsumerNodeId)
{
	const FGraphLayoutPreviewNodeSpec* EventSpec = FindNodeSpec(Sample, EventNodeId);
	const FGraphLayoutPreviewNodeSpec* ConsumerSpec = FindNodeSpec(Sample, ConsumerNodeId);
	if (!EventSpec || !ConsumerSpec)
	{
		Plan.Issues.Add(FString::Printf(
			TEXT("preview exec context is missing node spec: %s -> %s"),
			*EventNodeId,
			*ConsumerNodeId));
		return;
	}

	FVector2D EventTarget = FVector2D::ZeroVector;
	FVector2D ConsumerTarget = FVector2D::ZeroVector;
	if (!TryBuildSampleRelativeTarget(Sample, Plan, EventNodeId, EventTarget) ||
		!TryBuildSampleRelativeTarget(Sample, Plan, ConsumerNodeId, ConsumerTarget))
	{
		Plan.Issues.Add(FString::Printf(
			TEXT("preview exec context is missing snapshot node: %s -> %s"),
			*EventNodeId,
			*ConsumerNodeId));
		return;
	}

	const bool bEventPlaced = HasPlacementForNode(Plan, EventNodeId);
	const bool bConsumerPlaced = HasPlacementForNode(Plan, ConsumerNodeId);
	if (bConsumerPlaced && !bEventPlaced)
	{
		EventTarget.Y = ConsumerTarget.Y + GetAnchorOffset(*ConsumerSpec, ENodeRole::ExecNode).Y -
			GetAnchorOffset(*EventSpec, ENodeRole::EventEntry).Y;
	}
	else
	{
		ConsumerTarget.Y = EventTarget.Y + GetAnchorOffset(*EventSpec, ENodeRole::EventEntry).Y -
			GetAnchorOffset(*ConsumerSpec, ENodeRole::ExecNode).Y;
	}

	if (!bEventPlaced)
	{
		AddPlacement(
			Plan,
			Sample,
			*EventSpec,
			EventTarget,
			TEXT("preview_exec_context_straighten"));
	}
	if (!bConsumerPlaced)
	{
		AddPlacement(
			Plan,
			Sample,
			*ConsumerSpec,
			ConsumerTarget,
			TEXT("preview_exec_context_straighten"));
	}
}

void FGraphLayoutPreviewSemanticProjector::ProjectLinearExec(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	const FGraphLayoutPreviewNodeSpec* EventSpec = FindNodeSpec(Sample, TEXT("EventStart"));
	const FGraphLayoutPreviewNodeSpec* ResetSpec = FindNodeSpec(Sample, TEXT("ResetState"));
	const FVector2D* EventAnchor = SceneState.RoleCenters.Find(ENodeRole::EventEntry);
	const FVector2D* ExecAnchor = SceneState.RoleCenters.Find(ENodeRole::ExecNode);
	if (!EventSpec || !ResetSpec || !EventAnchor || !ExecAnchor)
	{
		Plan.Issues.Add(TEXT("linear preview sample is missing required semantic anchors"));
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		return;
	}

	AddPlacement(
		Plan,
		Sample,
		*EventSpec,
		BuildTopLeftFromAnchor(*EventSpec, ENodeRole::EventEntry, *EventAnchor),
		TEXT("preview_semantic_role_anchor"));
	AddPlacement(
		Plan,
		Sample,
		*ResetSpec,
		BuildTopLeftFromAnchor(*ResetSpec, ENodeRole::ExecNode, *ExecAnchor),
		TEXT("preview_semantic_role_anchor"));

	const TArray<FString> ChainNodeIds = {
		TEXT("SetCounter"),
		TEXT("PrintLabel"),
		TEXT("DelayAsync")
	};
	FVector2D PreviousExecAnchor = *ExecAnchor;
	for (const FString& NodeId : ChainNodeIds)
	{
		const FGraphLayoutPreviewNodeSpec* NodeSpec = FindNodeSpec(Sample, NodeId);
		if (!NodeSpec)
		{
			continue;
		}
		PreviousExecAnchor.X += RuleSet.ExecColumnSpacing;
		AddPlacement(
			Plan,
			Sample,
			*NodeSpec,
			BuildTopLeftFromAnchor(*NodeSpec, ENodeRole::ExecNode, PreviousExecAnchor),
			TEXT("preview_exec_pin_straighten"));
	}
}

void FGraphLayoutPreviewSemanticProjector::ProjectMultiExec(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	ProjectAnchoredNodesByRole(Sample, SceneState, Plan);

	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* PrimaryPlacement = FindPlacement(Plan, TEXT("PrimaryPrint"));
	const FNodePlacement* BranchPlacement = FindPlacement(Plan, TEXT("Branch"));
	const FGraphLayoutPreviewNodeSpec* EventSpec = FindNodeSpec(Sample, TEXT("EventStart"));
	const FGraphLayoutPreviewNodeSpec* PrimarySpec = FindNodeSpec(Sample, TEXT("PrimaryPrint"));
	const FGraphLayoutPreviewNodeSpec* BranchSpec = FindNodeSpec(Sample, TEXT("Branch"));
	if (!EventPlacement || !PrimaryPlacement || !BranchPlacement || !EventSpec || !PrimarySpec || !BranchSpec)
	{
		Plan.Issues.Add(TEXT("multi-exec preview sample is missing required semantic anchors"));
		return;
	}

	const FVector2D EventAnchor = EventPlacement->TargetPosition + GetAnchorOffset(*EventSpec, ENodeRole::EventEntry);
	const FVector2D PrimaryAnchor = PrimaryPlacement->TargetPosition + GetAnchorOffset(*PrimarySpec, ENodeRole::ExecNode);
	const FVector2D BranchAnchor = BranchPlacement->TargetPosition + GetAnchorOffset(*BranchSpec, ENodeRole::BranchControl);

	if (const FGraphLayoutPreviewNodeSpec* SequenceSpec = FindNodeSpec(Sample, TEXT("Sequence")))
	{
		const FVector2D SequenceAnchor((EventAnchor.X + PrimaryAnchor.X) * 0.5f, EventAnchor.Y);
		AddPlacement(
			Plan,
			Sample,
			*SequenceSpec,
			BuildTopLeftFromAnchor(*SequenceSpec, ENodeRole::BranchControl, SequenceAnchor),
			TEXT("preview_exec_pin_straighten"));
	}

	if (const FGraphLayoutPreviewNodeSpec* BranchConditionSpec = FindNodeSpec(Sample, TEXT("BranchCondition")))
	{
		const FVector2D ConditionCenter = BranchAnchor + FVector2D(-RuleSet.VariableInputOffsetX, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*BranchConditionSpec,
			BuildTopLeftFromAnchor(*BranchConditionSpec, ENodeRole::VariableInput, ConditionCenter),
			TEXT("preview_semantic_data_anchor"));
	}

	if (const FGraphLayoutPreviewNodeSpec* BranchPrintSpec = FindNodeSpec(Sample, TEXT("BranchPrint")))
	{
		const FVector2D BranchPrintAnchor = BranchAnchor + FVector2D(RuleSet.ExecColumnSpacing, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*BranchPrintSpec,
			BuildTopLeftFromAnchor(*BranchPrintSpec, ENodeRole::ExecNode, BranchPrintAnchor),
			TEXT("preview_exec_pin_straighten"));
	}

	if (const FGraphLayoutPreviewNodeSpec* CompletedPrintSpec = FindNodeSpec(Sample, TEXT("CompletedPrint")))
	{
		const FVector2D CompletedAnchor = PrimaryAnchor + FVector2D(RuleSet.ExecColumnSpacing, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*CompletedPrintSpec,
			BuildTopLeftFromAnchor(*CompletedPrintSpec, ENodeRole::ExecNode, CompletedAnchor),
			TEXT("preview_exec_pin_straighten"));
	}
}

void FGraphLayoutPreviewSemanticProjector::ProjectOccupancy(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	const FGraphLayoutPreviewNodeSpec* CandidateSpec = FindNodeSpec(Sample, TEXT("CandidateExec"));
	const FGraphLayoutPreviewNodeSpec* CommentSpec = FindNodeSpec(Sample, TEXT("CommentBlocker"));
	const FVector2D* CandidateAnchor = SceneState.RoleCenters.Find(ENodeRole::ExecNode);
	const FVector2D* CommentAnchor = SceneState.RoleCenters.Find(ENodeRole::Comment);
	const FVector2D* FallbackRowAnchor = SceneState.RoleCenters.Find(ENodeRole::AsyncNode);
	if (!CandidateSpec || !CommentSpec || !CandidateAnchor || !CommentAnchor || !FallbackRowAnchor)
	{
		Plan.Issues.Add(TEXT("occupancy preview sample is missing required semantic anchors"));
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		return;
	}

	AddPlacement(
		Plan,
		Sample,
		*CandidateSpec,
		BuildTopLeftFromAnchor(*CandidateSpec, ENodeRole::ExecNode, *CandidateAnchor),
		TEXT("preview_semantic_role_anchor"));
	AddPlacement(
		Plan,
		Sample,
		*CommentSpec,
		BuildTopLeftFromAnchor(*CommentSpec, ENodeRole::Comment, *CommentAnchor),
		TEXT("preview_semantic_role_anchor"));

	const float Step = FMath::Max(RuleSet.ExecColumnSpacing, CandidateSpec->Size.X + 32.0f);
	FVector2D ChainAnchor(CandidateAnchor->X + Step, FallbackRowAnchor->Y);

	if (const FGraphLayoutPreviewNodeSpec* FallbackSpec = FindNodeSpec(Sample, TEXT("FallbackExec")))
	{
		AddPlacement(
			Plan,
			Sample,
			*FallbackSpec,
			BuildTopLeftFromAnchor(*FallbackSpec, ENodeRole::ExecNode, ChainAnchor),
			TEXT("preview_occupancy_fallback_row"));
		ChainAnchor.X += Step;
	}

	if (const FGraphLayoutPreviewNodeSpec* DelaySpec = FindNodeSpec(Sample, TEXT("DelayAsync")))
	{
		AddPlacement(
			Plan,
			Sample,
			*DelaySpec,
			BuildTopLeftFromAnchor(*DelaySpec, ENodeRole::AsyncNode, ChainAnchor),
			TEXT("preview_occupancy_fallback_row"));
		ChainAnchor.X += Step;
	}

	if (const FGraphLayoutPreviewNodeSpec* ExistingGuardSpec = FindNodeSpec(Sample, TEXT("ExistingGuard")))
	{
		AddPlacement(
			Plan,
			Sample,
			*ExistingGuardSpec,
			BuildTopLeftFromAnchor(*ExistingGuardSpec, ENodeRole::ExecNode, ChainAnchor),
			TEXT("preview_occupancy_fallback_row"));
	}
}

FLayoutPlan FGraphLayoutPreviewSemanticProjector::Project(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet)
{
	FLayoutPlan Plan;
	Plan.Classifications = FClassifier::ClassifyGraph(Sample.Snapshot, RuleSet);
	const FEditorCanvasSceneState SceneState = FSemanticSceneAdapter::ResolveSceneState(RuleSet, Sample.Scene);

	switch (Sample.Scene)
	{
	case ESemanticScene::LinearExecChain:
		ProjectLinearExec(Sample, RuleSet, SceneState, Plan);
		break;
	case ESemanticScene::MultiExecOutput:
		ProjectMultiExec(Sample, RuleSet, SceneState, Plan);
		break;
	case ESemanticScene::PureDataSubgraph:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		ProjectExecContextLink(Sample, Plan, TEXT("EventStart"), TEXT("ConsumeArray"));
		break;
	case ESemanticScene::NodeInputCluster:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		ProjectExecContextLink(Sample, Plan, TEXT("EventStart"), TEXT("Consumer"));
		break;
	case ESemanticScene::Occupancy:
		ProjectOccupancy(Sample, RuleSet, SceneState, Plan);
		break;
	default:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		break;
	}

	ProjectEntryAvoidanceRangeComments(Sample, RuleSet, Plan);
	ProjectSemanticLabelComments(Sample, Plan);
	ProjectRemainingNodesBySampleOffset(Sample, Plan);
	return Plan;
}
}
