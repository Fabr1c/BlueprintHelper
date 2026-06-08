#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetRootRemovalPolicy
{
public:
	static bool Apply(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperRemoveRootWidgetRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);
};
