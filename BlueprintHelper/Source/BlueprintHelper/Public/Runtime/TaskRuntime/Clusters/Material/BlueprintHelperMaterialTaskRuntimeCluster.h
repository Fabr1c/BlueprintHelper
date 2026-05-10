// BlueprintHelper TaskRuntime - Material static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialTaskRuntimeCluster
{
public:
	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);
};
