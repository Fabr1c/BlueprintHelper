// BlueprintHelper TaskRuntime - Material static cluster

#include "Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.h"

bool FBlueprintHelperMaterialTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	static_cast<void>(LoweredStep);
	return false;
}
