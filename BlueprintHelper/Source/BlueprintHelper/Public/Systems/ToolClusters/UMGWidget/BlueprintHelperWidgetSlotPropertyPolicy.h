#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetSlotPropertyPolicy
{
public:
	static bool Apply(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperSetSlotPropertyRequest& Request,
		FBlueprintHelperWidgetMutationResult& OutResult);
};
