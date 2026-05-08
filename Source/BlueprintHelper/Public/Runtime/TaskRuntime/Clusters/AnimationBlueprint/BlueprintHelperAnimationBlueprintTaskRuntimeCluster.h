// BlueprintHelper TaskRuntime - AnimationBlueprint static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class BLUEPRINTHELPER_API FBlueprintHelperAnimationBlueprintTaskRuntimeCluster
{
public:
	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);
};
