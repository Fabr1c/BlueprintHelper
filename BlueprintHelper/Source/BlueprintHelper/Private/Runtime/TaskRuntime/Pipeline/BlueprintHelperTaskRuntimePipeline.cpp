#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Projection/BlueprintHelperTaskRuntimeResultProjection.h"

static TSharedRef<FJsonObject> MakePipelineStepResultJson(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult)
{
	TSharedRef<FJsonObject> StepJson = MakeShared<FJsonObject>();
	StepJson->SetStringField(TEXT("step_id"), LoweredStep.StepId);
	if (!LoweredStep.Capability.IsEmpty())
	{
		StepJson->SetStringField(TEXT("capability"), LoweredStep.Capability);
	}
	StepJson->SetStringField(TEXT("operation"), LoweredStep.RuntimeOperation);
	if (!LoweredStep.AdapterOperation.IsEmpty())
	{
		StepJson->SetStringField(TEXT("adapter_operation"), LoweredStep.AdapterOperation);
	}
	StepJson->SetStringField(TEXT("status"), ToolStatusToString(StepResult.Status));
	StepJson->SetObjectField(TEXT("result"), StepResult.ToJson());
	return StepJson;
}

static TSharedRef<FJsonObject> MakePipelinePostOperationResultJson(
	const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation)
{
	TSharedRef<FJsonObject> PostJson = MakeShared<FJsonObject>();
	PostJson->SetStringField(TEXT("operation"), PostOperation.Operation);
	PostJson->SetStringField(TEXT("status"), ToolStatusToString(PostOperation.Result.Status));
	if (!PostOperation.AssetPath.IsEmpty())
	{
		PostJson->SetStringField(TEXT("asset_path"), PostOperation.AssetPath);
	}
	if (!PostOperation.Status.IsEmpty())
	{
		PostJson->SetStringField(TEXT("post_status"), PostOperation.Status);
	}
	if (!PostOperation.Reason.IsEmpty())
	{
		PostJson->SetStringField(TEXT("reason"), PostOperation.Reason);
	}
	PostJson->SetNumberField(TEXT("duration_ms"), PostOperation.DurationMs);
	PostJson->SetObjectField(TEXT("result"), PostOperation.Result.ToJson());
	return PostJson;
}

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

FBlueprintHelperTaskRuntimePipeline::FBlueprintHelperTaskRuntimePipeline(
	TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> InExecutors)
	: Executors(MoveTemp(InExecutors))
{
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimePipeline::Execute(
	FBlueprintHelperTaskRuntimePipelineRunner& Runner,
	FBlueprintHelperTaskRuntimePipelineExecutionContext& ExecutionContext)
{
	FBlueprintHelperToolResultBase LastResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("task_runtime_pipeline"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	for (const TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>& Executor : Executors)
	{
		if (!Executor.IsValid())
		{
			continue;
		}

		LastResult = Runner.ExecuteStage(*Executor.Get(), ExecutionContext);
		if (!LastResult.bOk)
		{
			return LastResult;
		}
	}
	return LastResult;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePipeline::BuildRuntimeDataForStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FString& TaskRunId,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	bool bDryRun)
{
	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	StepRecords.Add({LoweredStep, StepResult});
	return BuildRuntimeDataForSteps(
		TaskPlan,
		TaskRunId,
		StepRecords,
		{},
		bDryRun);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePipeline::BuildRuntimeDataForSteps(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FString& TaskRunId,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
	bool bDryRun)
{
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRuntimeResult.v1"));
	if (!TaskRunId.IsEmpty())
	{
		Data->SetStringField(TEXT("task_run_id"), TaskRunId);
	}
	if (TaskPlan.IsValid())
	{
		FString TaskPlanSchema;
		if (TaskPlan->TryGetStringField(TEXT("schema"), TaskPlanSchema))
		{
			Data->SetStringField(TEXT("task_plan_schema"), TaskPlanSchema);
		}

		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
		{
			Data->SetArrayField(TEXT("target_assets"), *TargetAssets);
		}
	}

	TArray<TSharedPtr<FJsonValue>> Steps;
	for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
	{
		Steps.Add(MakeShared<FJsonValueObject>(MakePipelineStepResultJson(StepRecord.Step, StepRecord.Result)));
	}
	Data->SetArrayField(TEXT("steps"), Steps);

	if (PostOperationRecords.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> PostOperations;
		for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
		{
			PostOperations.Add(MakeShared<FJsonValueObject>(MakePipelinePostOperationResultJson(PostOperation)));
		}
		Data->SetArrayField(TEXT("post_operations"), PostOperations);
	}

	if (bDryRun)
	{
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			if (!StepRecord.Result.Data.IsValid())
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
			if (StepRecord.Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) &&
				DryRunObject && DryRunObject->IsValid())
			{
				Data->SetObjectField(TEXT("dry_run"), *DryRunObject);
				break;
			}
		}
	}

	return Data;
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

FBlueprintHelperTaskRuntimePipelineExecutionContext
FBlueprintHelperTaskRuntimePipelineRunner::MakeExecutionContext() const
{
	FBlueprintHelperTaskRuntimePipelineExecutionContext ExecutionContext;
	ExecutionContext.TaskPlan = Context.TaskPlan;
	ExecutionContext.TaskRunId = Context.TaskRunId;
	ExecutionContext.bDryRun = Context.bDryRun;
	ExecutionContext.StepRecords = Context.StepRecords;
	return ExecutionContext;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimePipelineRunner::ExecuteStage(
	IBlueprintHelperTaskRuntimePipelineStageExecutor& Executor,
	FBlueprintHelperTaskRuntimePipelineExecutionContext& ExecutionContext)
{
	RecordStageOnce(Executor.GetStage());
	return Executor.Execute(ExecutionContext);
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

bool FBlueprintHelperTaskRuntimePipelineRunner::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeClusterHub& ClusterHub,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence);
	return ClusterHub.BuildReviewEvidence(
		LoweredStep,
		StepResult,
		ArchiveSessionId,
		TaskRunId,
		StepIndex,
		OutEvidence);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePipelineRunner::BuildRuntimeData(
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& InStepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& InPostOperationRecords)
{
	RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage::ProjectMetricsAndResult);
	SetStepRecords(InStepRecords);
	return FBlueprintHelperTaskRuntimePipeline::BuildRuntimeDataForSteps(
		Context.TaskPlan,
		Context.TaskRunId,
		InStepRecords,
		InPostOperationRecords,
		Context.bDryRun);
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
