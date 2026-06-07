#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

#include "Runtime/TaskRuntime/Projection/BlueprintHelperTaskRuntimeResultProjection.h"

const TCHAR* FBlueprintHelperTaskRuntimePipelineStageNames::ToString(
	EBlueprintHelperTaskRuntimePipelineStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract:
		return TEXT("validate_compiled_plan_contract");
	case EBlueprintHelperTaskRuntimePipelineStage::ResolveBridgeRoute:
		return TEXT("resolve_bridge_route");
	case EBlueprintHelperTaskRuntimePipelineStage::ResolveClusterFamilyAdapter:
		return TEXT("resolve_cluster_family_adapter");
	case EBlueprintHelperTaskRuntimePipelineStage::ExecuteCluster:
		return TEXT("execute_cluster");
	case EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence:
		return TEXT("build_review_evidence");
	case EBlueprintHelperTaskRuntimePipelineStage::ProjectMetricsAndResult:
		return TEXT("project_metrics_and_result");
	case EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations:
		return TEXT("run_post_operations");
	case EBlueprintHelperTaskRuntimePipelineStage::BuildJournal:
		return TEXT("build_journal");
	case EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse:
		return TEXT("finalize_bridge_response");
	default:
		return TEXT("unknown");
	}
}

void FBlueprintHelperTaskRuntimePipelineContext::RecordStage(
	EBlueprintHelperTaskRuntimePipelineStage Stage)
{
	FBlueprintHelperTaskRuntimePipelineStageTrace Trace;
	Trace.Stage = Stage;
	Trace.Name = FBlueprintHelperTaskRuntimePipelineStageNames::ToString(Stage);
	StageTrace.Add(MoveTemp(Trace));
}

bool FBlueprintHelperTaskRuntimePipelineContext::HasStage(
	EBlueprintHelperTaskRuntimePipelineStage Stage) const
{
	return StageTrace.ContainsByPredicate(
		[Stage](const FBlueprintHelperTaskRuntimePipelineStageTrace& Trace)
		{
			return Trace.Stage == Stage;
		});
}

const TArray<FBlueprintHelperTaskRuntimePipelineStageTrace>&
FBlueprintHelperTaskRuntimePipelineContext::GetStageTrace() const
{
	return StageTrace;
}

TArray<EBlueprintHelperTaskRuntimePipelineStage>
FBlueprintHelperTaskRuntimePipeline::GetDefaultStageOrder()
{
	return {
		EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract,
		EBlueprintHelperTaskRuntimePipelineStage::ResolveBridgeRoute,
		EBlueprintHelperTaskRuntimePipelineStage::ResolveClusterFamilyAdapter,
		EBlueprintHelperTaskRuntimePipelineStage::ExecuteCluster,
		EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence,
		EBlueprintHelperTaskRuntimePipelineStage::ProjectMetricsAndResult,
		EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations,
		EBlueprintHelperTaskRuntimePipelineStage::BuildJournal,
		EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse,
	};
}

FBlueprintHelperTaskRuntimePipelineRunner::FBlueprintHelperTaskRuntimePipelineRunner(
	const TSharedPtr<FJsonObject>& InTaskPlan,
	const FString& InTaskRunId,
	bool bInDryRun)
{
	Context.TaskPlan = InTaskPlan;
	Context.TaskRunId = InTaskRunId;
	Context.bDryRun = bInDryRun;
}

void FBlueprintHelperTaskRuntimePipelineRunner::RecordStageOnce(
	EBlueprintHelperTaskRuntimePipelineStage Stage)
{
	if (!Context.HasStage(Stage))
	{
		Context.RecordStage(Stage);
	}
}

bool FBlueprintHelperTaskRuntimePipelineRunner::HasStage(
	EBlueprintHelperTaskRuntimePipelineStage Stage) const
{
	return Context.HasStage(Stage);
}

void FBlueprintHelperTaskRuntimePipelineRunner::SetStepRecords(
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& InStepRecords)
{
	Context.StepRecords = InStepRecords;
}

void FBlueprintHelperTaskRuntimePipelineRunner::AttachToResult(
	FBlueprintHelperToolResultBase& RuntimeResult) const
{
	FBlueprintHelperTaskRuntimeResultProjection::AttachPipelineTrace(
		RuntimeResult.Data,
		Context);
}

const FBlueprintHelperTaskRuntimePipelineContext&
FBlueprintHelperTaskRuntimePipelineRunner::GetContext() const
{
	return Context;
}
