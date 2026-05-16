// BlueprintHelper TaskRuntime - ObjectProperty static cluster.

#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

FBlueprintHelperObjectPropertyTaskRuntimeCluster::FBlueprintHelperObjectPropertyTaskRuntimeCluster(
	const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService)
	: PropertyReflectionService(InPropertyReflectionService)
{
}

bool FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty;
}

bool FBlueprintHelperObjectPropertyTaskRuntimeCluster::BuildReviewEvidence(
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

FBlueprintHelperToolResultBase FBlueprintHelperObjectPropertyTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteObjectPropertyTaskPlanStep(
		PropertyReflectionService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
