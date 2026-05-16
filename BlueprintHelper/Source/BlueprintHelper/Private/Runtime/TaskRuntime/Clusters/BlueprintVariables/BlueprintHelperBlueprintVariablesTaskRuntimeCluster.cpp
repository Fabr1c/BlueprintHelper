// BlueprintHelper TaskRuntime - BlueprintVariables static cluster.

#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"

FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::FBlueprintHelperBlueprintVariablesTaskRuntimeCluster(
	const FBlueprintHelperBlueprintVariableService& InVariableService)
	: VariableService(InVariableService)
{
}

bool FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperBlueprintVariableTaskPlanAdapter::CapabilityBlueprintVariable ||
		LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationAddMemberVariables ||
		LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch;
}

bool FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::BuildReviewEvidence(
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

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	if (LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationAddMemberVariables)
	{
		return VariableService.AddMemberVariables(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch)
	{
		return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteBlueprintVariableBatchTaskPlanStep(VariableService, LoweredStep.Payload);
	}

	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::MakeFailure(
		TEXT("blueprint_variable"),
		TEXT("unsupported_variable_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported BlueprintVariables adapter operation."),
		TEXT("task_plan.steps[0]"));
}
