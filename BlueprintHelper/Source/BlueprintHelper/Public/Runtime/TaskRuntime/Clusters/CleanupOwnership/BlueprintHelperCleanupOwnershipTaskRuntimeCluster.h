// BlueprintHelper TaskRuntime - CleanupOwnership static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperCleanupBlueprintHelperBlockService;
class FBlueprintHelperRollbackCleanupTransactionService;
class FBlueprintHelperConvertBlockToUserOwnedService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperCleanupOwnershipTaskRuntimeCluster
{
public:
	FBlueprintHelperCleanupOwnershipTaskRuntimeCluster(
		const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
		const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
		const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService);

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
	const FBlueprintHelperCleanupBlueprintHelperBlockService& CleanupBlockService;
	const FBlueprintHelperRollbackCleanupTransactionService& RollbackCleanupService;
	const FBlueprintHelperConvertBlockToUserOwnedService& ConvertBlockService;
};
