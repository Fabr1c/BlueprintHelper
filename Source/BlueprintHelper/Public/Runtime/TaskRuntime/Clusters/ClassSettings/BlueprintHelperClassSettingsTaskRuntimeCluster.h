// BlueprintHelper TaskRuntime - ClassSettings static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperClassSettingsService;

class BLUEPRINTHELPER_API FBlueprintHelperClassSettingsTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperClassSettingsTaskRuntimeCluster(
		const FBlueprintHelperClassSettingsService& InClassSettingsService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperClassSettingsService& ClassSettingsService;
};
