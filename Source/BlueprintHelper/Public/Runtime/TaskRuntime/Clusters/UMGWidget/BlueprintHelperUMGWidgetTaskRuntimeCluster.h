// BlueprintHelper TaskRuntime - UMGWidget static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperWidgetService;

class BLUEPRINTHELPER_API FBlueprintHelperUMGWidgetTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperUMGWidgetTaskRuntimeCluster(
		const FBlueprintHelperWidgetService& InWidgetService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperWidgetService& WidgetService;
};
