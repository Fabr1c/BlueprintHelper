#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewOverlayProjector
{
public:
	static void AppendOverlays(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		FLayoutPlan& Plan);

private:
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
	static void AppendEntryAvoidanceRangeComments(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		FLayoutPlan& Plan);
	static void AppendSemanticLabelComments(
		const FGraphLayoutPreviewSample& Sample,
		FLayoutPlan& Plan);
};
}
