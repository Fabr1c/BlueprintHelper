#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetVariablePolicy
{
public:
	static bool Apply(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperSetWidgetAsVariableRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);
};
