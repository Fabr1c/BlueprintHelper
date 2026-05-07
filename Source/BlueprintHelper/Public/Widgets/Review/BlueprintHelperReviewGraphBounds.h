// Review graph diff bounds helpers.

#pragma once

#include "CoreMinimal.h"
#include "Structure/Review/BlueprintHelperReviewTypes.h"

class SGraphEditor;
class UEdGraph;
class UEdGraphNode;

namespace BlueprintHelperReviewGraphBounds
{
	bool DoesNodeMatchTargetKey(const UEdGraphNode* Node, const FString& TargetKey);

	bool BuildCommentStyleBoundsForNodes(
		const TArray<UEdGraphNode*>& Nodes,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FVector2D& OutPosition,
		FVector2D& OutSize);

	bool BuildBoundsForTargets(
		const TArray<FBlueprintHelperReviewAtomicTarget>& Targets,
		const UEdGraph* Graph,
		const FString& GraphName,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FVector2D& OutPosition,
		FVector2D& OutSize,
		FString* OutDebugSummary = nullptr);
}
