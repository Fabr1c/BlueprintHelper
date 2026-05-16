// BlueprintHelper TaskRuntime cluster hub utilities.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"

class FJsonObject;

class FBlueprintHelperTaskRuntimeClusterHubUtils
{
public:
	using FCanExecutePredicate = bool (*)(const FBlueprintHelperTaskRuntimeLoweredStep&);

	static EBlueprintHelperTaskRuntimeCluster ResolveClusterForLoweredStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	static TSharedRef<FJsonObject> MakeSyntheticDryRunData();

	static FBlueprintHelperToolResultBase MakeUnsupportedAdapterOperationResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);
};
