// BlueprintHelper TaskRuntime - AnimationBlueprint static cluster

#include "Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.h"

bool FBlueprintHelperAnimationBlueprintTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	static_cast<void>(LoweredStep);
	return false;
}
