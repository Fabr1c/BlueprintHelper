#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewSemanticProjector
{
public:
	static FLayoutPlan Project(const FGraphLayoutPreviewSample& Sample, const FRuleSet& RuleSet);

private:
	static FVector2D GetAnchorOffset(const FGraphLayoutPreviewNodeSpec& NodeSpec, ENodeRole AnchorRole);
	static FVector2D BuildTopLeftFromAnchor(
		const FGraphLayoutPreviewNodeSpec& NodeSpec,
		ENodeRole AnchorRole,
		const FVector2D& AnchorPosition);
	static const FGraphLayoutPreviewNodeSpec* FindNodeSpec(
		const FGraphLayoutPreviewSample& Sample,
		const FString& NodeId);
	static const FNodeSnapshot* FindSnapshotNode(
		const FGraphLayoutPreviewSample& Sample,
		const FString& NodeId);
	static const FNodePlacement* FindPlacement(
		const FLayoutPlan& Plan,
		const FString& NodeId);
	static bool HasPlacementForNode(
		const FLayoutPlan& Plan,
		const FString& NodeId);
	static void AddPlacement(
		FLayoutPlan& Plan,
		const FGraphLayoutPreviewSample& Sample,
		const FGraphLayoutPreviewNodeSpec& NodeSpec,
		const FVector2D& TargetPosition,
		const FString& Reason,
		const FVector2D& TargetSize = FVector2D::ZeroVector);
	static void ProjectAnchoredNodesByRole(
		const FGraphLayoutPreviewSample& Sample,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectRemainingNodesBySampleOffset(
		const FGraphLayoutPreviewSample& Sample,
		FLayoutPlan& Plan);
	static bool TryBuildSampleRelativeTarget(
		const FGraphLayoutPreviewSample& Sample,
		const FLayoutPlan& Plan,
		const FString& NodeId,
		FVector2D& OutTargetPosition);
	static void ProjectExecContextLink(
		const FGraphLayoutPreviewSample& Sample,
		FLayoutPlan& Plan,
		const FString& EventNodeId,
		const FString& ConsumerNodeId);
	static void ProjectLinearExec(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectMultiExec(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectOccupancy(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectEntryAvoidanceRangeComments(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		FLayoutPlan& Plan);
};
}
