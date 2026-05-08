// BlueprintHelper TaskRuntime - Signature static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperBlueprintStructureService;

class BLUEPRINTHELPER_API FBlueprintHelperSignatureTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperSignatureTaskRuntimeCluster(
		const FBlueprintHelperBlueprintStructureService& InStructureService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperBlueprintStructureService& StructureService;
};
