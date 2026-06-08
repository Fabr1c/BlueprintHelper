#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetSubtreeClonePolicy
{
public:
	static bool DuplicateSubtree(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperDuplicateWidgetSubtreeRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);

	static bool WrapWidget(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperWrapWidgetRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);

	static bool ReplaceWidgetClass(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperReplaceWidgetClassRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);
};
