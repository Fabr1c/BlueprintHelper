// BlueprintHelper TaskRuntime - MaterialInstance static cluster.

#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/MaterialInstance/BlueprintHelperMaterialInstanceTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceMutationAdapter.h"

bool FBlueprintHelperMaterialInstanceTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperMaterialInstanceTaskPlanAdapter::CapabilityMaterialInstance;
}

bool FBlueprintHelperMaterialInstanceTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	return StepResult.bOk && FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
		LoweredStep,
		ArchiveSessionId,
		TaskRunId,
		StepIndex,
		OutEvidence);
}

FBlueprintHelperToolResultBase FBlueprintHelperMaterialInstanceTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperMaterialInstanceMutationAdapter::ExecutePayload(LoweredStep.Payload);
}
