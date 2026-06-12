// BlueprintHelper TaskRuntime - GraphWrite static cluster

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteReviewEvidenceBuilder.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"

static FBlueprintHelperToolError BlueprintHelperGraphWriteTaskRuntimeClusterMakeUnsupportedGraphWriteOperationError()
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("unsupported_taskplan_adapter_operation");
	Error.Stage = EBlueprintHelperToolStage::ParseInput;
	Error.Message = TEXT("Task Runtime GraphWrite cluster received an unsupported adapter operation.");
	Error.Field = TEXT("task_plan.steps[0]");
	return Error;
}

static EBlueprintHelperGraphBodyKind BlueprintHelperGraphWriteTaskRuntimeClusterInferBodyKind(
	const FString& AdapterOperation)
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	if (FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(AdapterOperation, Descriptor) ||
		FBlueprintHelperGraphBodyAdapterRegistry::TryFindByTaskSpecStrategy(AdapterOperation, Descriptor))
	{
		return Descriptor.BodyKind;
	}
	if (FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(AdapterOperation))
	{
		return EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	}
	return EBlueprintHelperGraphBodyKind::Unknown;
}

FBlueprintHelperGraphWriteTaskRuntimeCluster::FBlueprintHelperGraphWriteTaskRuntimeCluster(
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry)
	: GraphWriteRegistry(InGraphWriteRegistry)
{
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(LoweredStep.AdapterOperation) ||
		LoweredStep.Capability == TEXT("graph_write");
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	FBlueprintHelperGraphWriteReviewEvidenceBuildInput Input;
	Input.LoweredStep = LoweredStep;
	Input.StepResult = StepResult;
	Input.ArchiveSessionId = ArchiveSessionId;
	Input.TaskRunId = TaskRunId;
	Input.StepIndex = StepIndex;
	Input.BoundaryModel.RuntimeAdapterId = LoweredStep.AdapterOperation;
	Input.BoundaryModel.TaskSpecStrategy = LoweredStep.RuntimeOperation;
	Input.BoundaryModel.BodyKind =
		BlueprintHelperGraphWriteTaskRuntimeClusterInferBodyKind(LoweredStep.AdapterOperation);
	return FBlueprintHelperGraphWriteReviewEvidenceBuilder::Build(Input, OutEvidence);
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	if (FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(LoweredStep.AdapterOperation))
	{
		return GraphWriteRegistry.Execute(LoweredStep.AdapterOperation, LoweredStep.Payload.ToSharedRef());
	}

	return FBlueprintHelperToolResultBuilder::Failure(
		LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		BlueprintHelperGraphWriteTaskRuntimeClusterMakeUnsupportedGraphWriteOperationError());
}
