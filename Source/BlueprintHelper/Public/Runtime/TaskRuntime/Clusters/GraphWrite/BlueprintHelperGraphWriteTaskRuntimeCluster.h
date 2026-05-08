// BlueprintHelper TaskRuntime - GraphWrite static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteTaskRuntimeCluster
{
public:
	FBlueprintHelperGraphWriteTaskRuntimeCluster(
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
		const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
		const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
		const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperAppendBlueprintGraphService& AppendGraphService;
	const FBlueprintHelperReplaceBlueprintGraphService& ReplaceGraphService;
	const FBlueprintHelperPatchBlueprintGraphService& PatchGraphService;
	const FBlueprintHelperMergeBlueprintGraphService& MergeGraphService;
};
