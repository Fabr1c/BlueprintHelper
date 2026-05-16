// BlueprintHelper TaskRuntime - GraphWrite static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteTaskRuntimeCluster
{
public:
	FBlueprintHelperGraphWriteTaskRuntimeCluster(
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
		const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
		const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
		const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	static bool BuildReviewEvidence(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperAppendBlueprintGraphService& AppendGraphService;
	const FBlueprintHelperReplaceBlueprintGraphService& ReplaceGraphService;
	const FBlueprintHelperPatchBlueprintGraphService& PatchGraphService;
	const FBlueprintHelperMergeBlueprintGraphService& MergeGraphService;
};
