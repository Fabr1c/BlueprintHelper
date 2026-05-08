// BlueprintHelper TaskRuntime - BlueprintVariables static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperBlueprintVariableService;

class BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariablesTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperBlueprintVariablesTaskRuntimeCluster(
		const FBlueprintHelperBlueprintVariableService& InVariableService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperBlueprintVariableService& VariableService;
};
