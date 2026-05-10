// Review graph diff bounds helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class SGraphEditor;
class UEdGraph;
class UEdGraphNode;

class BLUEPRINTHELPER_API FBlueprintHelperReviewGraphBounds
{
public:
	static bool DoesNodeMatchTargetKey(const UEdGraphNode* Node, const FString& TargetKey);

	static bool BuildCommentStyleBoundsForNodes(
		const TArray<UEdGraphNode*>& Nodes,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FVector2D& OutPosition,
		FVector2D& OutSize);

	static bool BuildBoundsForTargets(
		const TArray<FBlueprintHelperReviewAtomicTarget>& Targets,
		const UEdGraph* Graph,
		const FString& GraphName,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FVector2D& OutPosition,
		FVector2D& OutSize,
		FString* OutDebugSummary = nullptr);
};
