#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class FBlueprintHelperClassSettingsService;
class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetBlueprintReparentPolicy
{
public:
	static bool Apply(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperReparentWidgetBlueprintRequest& Request,
		const FBlueprintHelperClassSettingsService* ClassSettingsService,
		FBlueprintHelperWidgetMutationResult& OutResult);
};
