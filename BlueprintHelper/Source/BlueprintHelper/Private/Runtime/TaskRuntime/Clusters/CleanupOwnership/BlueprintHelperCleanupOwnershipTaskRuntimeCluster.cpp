// BlueprintHelper TaskRuntime - CleanupOwnership static cluster.

#include "Runtime/TaskRuntime/Clusters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::FBlueprintHelperCleanupOwnershipTaskRuntimeCluster(
	const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
	const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
	const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService)
	: CleanupBlockService(InCleanupBlockService)
	, RollbackCleanupService(InRollbackCleanupService)
	, ConvertBlockService(InConvertBlockService)
{
}

bool FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::CapabilityName;
}

bool FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep&,
	const FBlueprintHelperToolResultBase&,
	const FString&,
	const FString&,
	int32,
	FBlueprintHelperWriteReviewEvidence&)
{
	return false;
}

FBlueprintHelperToolResultBase FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteCleanupOwnershipTaskPlanStep(
		CleanupBlockService,
		ConvertBlockService,
		RollbackCleanupService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
