#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"

namespace BlueprintHelper::GraphLayout
{
FVector2D FGraphLayoutPreviewSemanticProjector::GetAnchorOffset(
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const ENodeRole AnchorRole)
{
	const FVector2D Size = NodeSpec.Size;
	switch (AnchorRole)
	{
	case ENodeRole::EventEntry:
		return FVector2D(FMath::Max(0.0f, Size.X - 18.0f), 58.0f);
	case ENodeRole::ExecNode:
	case ENodeRole::BranchControl:
	case ENodeRole::AsyncNode:
	case ENodeRole::DelegateNode:
		return FVector2D(16.0f, 48.0f);
	case ENodeRole::PureFunction:
	case ENodeRole::OperatorOrCompare:
	case ENodeRole::VariableInput:
	case ENodeRole::Comment:
	case ENodeRole::Unknown:
	default:
		return Size * 0.5f;
	}
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
	const FString& Reason)
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
	Placement.bMoveExisting = true;
	Placement.Reason = Reason;
	Plan.Placements.Add(Placement);
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
	case ESemanticScene::NodeInputCluster:
	case ESemanticScene::Occupancy:
	default:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		break;
	}

	ProjectRemainingNodesBySampleOffset(Sample, Plan);
	return Plan;
}
}
