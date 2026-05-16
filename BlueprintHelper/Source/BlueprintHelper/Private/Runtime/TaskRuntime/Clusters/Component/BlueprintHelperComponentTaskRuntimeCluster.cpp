// BlueprintHelper TaskRuntime - Component static cluster.

#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

FBlueprintHelperComponentTaskRuntimeCluster::FBlueprintHelperComponentTaskRuntimeCluster(
	const FBlueprintHelperComponentService& InComponentService)
	: ComponentService(InComponentService)
{
}

bool FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent;
}

bool FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence(
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

FBlueprintHelperToolResultBase FBlueprintHelperComponentTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteComponentTaskPlanStep(
		ComponentService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
