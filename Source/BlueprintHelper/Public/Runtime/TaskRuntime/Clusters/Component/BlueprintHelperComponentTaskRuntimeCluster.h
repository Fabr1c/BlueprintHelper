// BlueprintHelper TaskRuntime - Component static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperComponentService;

class BLUEPRINTHELPER_API FBlueprintHelperComponentTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperComponentTaskRuntimeCluster(
		const FBlueprintHelperComponentService& InComponentService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperComponentService& ComponentService;
};
