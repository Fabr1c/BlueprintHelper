// BlueprintHelper TaskRuntime - ObjectProperty static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperPropertyReflectionService;

class BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperObjectPropertyTaskRuntimeCluster(
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperPropertyReflectionService& PropertyReflectionService;
};
