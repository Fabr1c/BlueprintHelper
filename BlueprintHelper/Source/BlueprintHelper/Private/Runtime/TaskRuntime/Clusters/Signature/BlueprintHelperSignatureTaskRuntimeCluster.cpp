// BlueprintHelper TaskRuntime - Signature static cluster.

#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

FBlueprintHelperSignatureTaskRuntimeCluster::FBlueprintHelperSignatureTaskRuntimeCluster(
	const FBlueprintHelperBlueprintStructureService& InStructureService)
	: StructureService(InStructureService)
{
}

bool FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperSignatureTaskPlanAdapter::CapabilityName;
}

bool FBlueprintHelperSignatureTaskRuntimeCluster::BuildReviewEvidence(
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

FBlueprintHelperToolResultBase FBlueprintHelperSignatureTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteSignatureTaskPlanStep(
		StructureService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
