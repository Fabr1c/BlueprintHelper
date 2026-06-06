#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

const TCHAR* FBlueprintHelperTaskRuntimePipelineStageNames::ToString(
	EBlueprintHelperTaskRuntimePipelineStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperTaskRuntimePipelineStage::Prepare:
		return TEXT("prepare");
	case EBlueprintHelperTaskRuntimePipelineStage::ResolvePreviewToken:
		return TEXT("resolve_preview_token");
	case EBlueprintHelperTaskRuntimePipelineStage::CaptureReviewBaseline:
		return TEXT("capture_review_baseline");
	case EBlueprintHelperTaskRuntimePipelineStage::ExecuteSteps:
		return TEXT("execute_steps");
	case EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence:
		return TEXT("build_review_evidence");
	case EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations:
		return TEXT("run_post_operations");
	case EBlueprintHelperTaskRuntimePipelineStage::BuildJournal:
		return TEXT("build_journal");
	case EBlueprintHelperTaskRuntimePipelineStage::AttachRuntimeFacts:
		return TEXT("attach_runtime_facts");
	case EBlueprintHelperTaskRuntimePipelineStage::FinalizeResult:
		return TEXT("finalize_result");
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
		EBlueprintHelperTaskRuntimePipelineStage::Prepare,
		EBlueprintHelperTaskRuntimePipelineStage::ResolvePreviewToken,
		EBlueprintHelperTaskRuntimePipelineStage::CaptureReviewBaseline,
		EBlueprintHelperTaskRuntimePipelineStage::ExecuteSteps,
		EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence,
		EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations,
		EBlueprintHelperTaskRuntimePipelineStage::BuildJournal,
		EBlueprintHelperTaskRuntimePipelineStage::AttachRuntimeFacts,
		EBlueprintHelperTaskRuntimePipelineStage::FinalizeResult,
	};
}
