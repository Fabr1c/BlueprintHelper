// BlueprintHelper TaskRuntime commit result DTO.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

struct FBlueprintHelperTaskRuntimeCommitResult
{
	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PostOperationRecords;
	FBlueprintHelperValidationSummary BaseValidation;
	bool bSawStepValidation = false;
	bool bSawExecutionFailure = false;
	bool bHasFirstExecutionError = false;
	FBlueprintHelperToolError FirstExecutionError;
};
