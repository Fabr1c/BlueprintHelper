#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationTypes.h"

class FBlueprintHelperTaskRuntimeCommitService;

struct FBlueprintHelperTaskRuntimePostOperationExecutionResult
{
	bool bOk = true;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecordEx> Records;
	TOptional<FBlueprintHelperToolError> FirstError;
};

class FBlueprintHelperTaskRuntimePostOperationExecutor
{
public:
	FBlueprintHelperTaskRuntimePostOperationExecutionResult Execute(
		const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
		const FBlueprintHelperTaskRuntimeCommitService* CommitService) const;

private:
	static FBlueprintHelperToolResultBase MakeFallbackSkippedResult(
		const FString& Operation,
		const FString& AssetPath,
		const FString& Reason);
};
