// BlueprintHelper TaskRuntime - GraphWrite static cluster

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"

#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"

namespace
{
	FBlueprintHelperToolError MakeUnsupportedGraphWriteOperationError()
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("unsupported_taskplan_adapter_operation");
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = TEXT("Task Runtime GraphWrite cluster received an unsupported adapter operation.");
		Error.Field = TEXT("task_plan.steps[0]");
		return Error;
	}
}

FBlueprintHelperGraphWriteTaskRuntimeCluster::FBlueprintHelperGraphWriteTaskRuntimeCluster(
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService)
	: AppendGraphService(InAppendGraphService)
	, ReplaceGraphService(InReplaceGraphService)
	, PatchGraphService(InPatchGraphService)
	, MergeGraphService(InMergeGraphService)
{
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.AdapterOperation == TEXT("append_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("replace_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("patch_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("merge_blueprint_graph") ||
		LoweredStep.Capability == TEXT("graph_write");
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep&,
	const FBlueprintHelperToolResultBase&,
	const FString&,
	const FString&,
	int32,
	FBlueprintHelperWriteReviewEvidence&)
{
	return false;
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	if (LoweredStep.AdapterOperation == TEXT("append_blueprint_graph"))
	{
		return AppendGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("replace_blueprint_graph"))
	{
		return ReplaceGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("patch_blueprint_graph"))
	{
		return PatchGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("merge_blueprint_graph"))
	{
		return MergeGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}

	return FBlueprintHelperToolResultBuilder::Failure(
		LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		MakeUnsupportedGraphWriteOperationError());
}
