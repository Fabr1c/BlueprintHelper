#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipelineExecutors.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationPlanner.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

static FBlueprintHelperToolResultBase BlueprintHelperMakeTaskRuntimePipelineStageResult(
	const FString& Operation)
{
	return FBlueprintHelperToolResultBuilder::Applied(
		Operation.IsEmpty() ? TEXT("task_runtime_pipeline_stage") : Operation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
}

FBlueprintHelperTaskRuntimeCallbackStageExecutor::FBlueprintHelperTaskRuntimeCallbackStageExecutor(
	EBlueprintHelperTaskRuntimePipelineStage InStage,
	FCallback InCallback)
	: Stage(InStage)
	, Callback(MoveTemp(InCallback))
{
}

EBlueprintHelperTaskRuntimePipelineStage FBlueprintHelperTaskRuntimeCallbackStageExecutor::GetStage() const
{
	return Stage;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCallbackStageExecutor::Execute(
	FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
{
	if (Callback)
	{
		return Callback(Context);
	}

	return BlueprintHelperMakeTaskRuntimePipelineStageResult(
		FBlueprintHelperTaskRuntimePipelineStageNames::ToString(Stage));
}

FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor::FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor(
	const FBlueprintHelperTaskRuntimeCommitService& InCommitService,
	const FBlueprintHelperTaskRuntimeLoweredStep& InLoweredStep,
	bool bInDryRun)
	: CommitService(InCommitService)
	, LoweredStep(InLoweredStep)
	, bDryRun(bInDryRun)
{
}

EBlueprintHelperTaskRuntimePipelineStage FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor::GetStage() const
{
	return EBlueprintHelperTaskRuntimePipelineStage::ExecuteCluster;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor::Execute(
	FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
{
	static_cast<void>(Context);
	return CommitService.ExecuteStep(LoweredStep, bDryRun);
}

FBlueprintHelperTaskRuntimePostOperationStageExecutor::FBlueprintHelperTaskRuntimePostOperationStageExecutor(
	const FBlueprintHelperTaskRuntimePostOperationPlan& InPlan,
	EBlueprintHelperTaskRuntimePostOperationKind InKind,
	const FBlueprintHelperTaskRuntimeCommitService* InCommitService)
	: Plan(FilterPlanByKind(InPlan, InKind))
	, Kind(InKind)
	, CommitService(InCommitService)
{
}

EBlueprintHelperTaskRuntimePipelineStage FBlueprintHelperTaskRuntimePostOperationStageExecutor::GetStage() const
{
	return EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimePostOperationStageExecutor::Execute(
	FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
{
	ExecutionResult = FBlueprintHelperTaskRuntimePostOperationExecutionResult();
	if (Plan.Items.Num() == 0)
	{
		return BlueprintHelperMakeTaskRuntimePipelineStageResult(TEXT("task_runtime_post_operation"));
	}

	FBlueprintHelperTaskRuntimePostOperationExecutor Executor;
	ExecutionResult = Executor.Execute(Plan, CommitService);
	for (const FBlueprintHelperTaskRuntimePostOperationRecordEx& Record : ExecutionResult.Records)
	{
		Context.PostOperationRecords.Add(ConvertRecord(Record));
	}

	if (ExecutionResult.bOk)
	{
		return BlueprintHelperMakeTaskRuntimePipelineStageResult(TEXT("task_runtime_post_operation"));
	}

	FBlueprintHelperToolError Error;
	if (ExecutionResult.FirstError.IsSet())
	{
		Error = *ExecutionResult.FirstError;
	}
	else
	{
		Error.Code = TEXT("task_post_operation_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("TaskPlan post operation failed.");
	}
	return FBlueprintHelperToolResultBuilder::Failure(
		TEXT("task_runtime_post_operation"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		Error);
}

const FBlueprintHelperTaskRuntimePostOperationExecutionResult&
FBlueprintHelperTaskRuntimePostOperationStageExecutor::GetExecutionResult() const
{
	return ExecutionResult;
}

FBlueprintHelperTaskRuntimePostOperationPlan
FBlueprintHelperTaskRuntimePostOperationStageExecutor::FilterPlanByKind(
	const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
	EBlueprintHelperTaskRuntimePostOperationKind Kind)
{
	FBlueprintHelperTaskRuntimePostOperationPlan FilteredPlan;
	FilteredPlan.bRequestedCompile =
		Kind == EBlueprintHelperTaskRuntimePostOperationKind::Compile && Plan.bRequestedCompile;
	FilteredPlan.bRequestedSave =
		Kind == EBlueprintHelperTaskRuntimePostOperationKind::Save && Plan.bRequestedSave;
	FilteredPlan.bHasTargetAssets = Plan.bHasTargetAssets;
	FilteredPlan.MissingTargetAssetsReason = Plan.MissingTargetAssetsReason;
	for (const FBlueprintHelperTaskRuntimePostOperationPlanItem& Item : Plan.Items)
	{
		if (Item.Kind == Kind)
		{
			FilteredPlan.Items.Add(Item);
		}
	}
	return FilteredPlan;
}

FBlueprintHelperTaskRuntimePostOperationRecord
FBlueprintHelperTaskRuntimePostOperationStageExecutor::ConvertRecord(
	const FBlueprintHelperTaskRuntimePostOperationRecordEx& Record)
{
	FBlueprintHelperTaskRuntimePostOperationRecord RuntimeRecord;
	RuntimeRecord.Operation = Record.Operation;
	RuntimeRecord.Result = Record.Result;
	RuntimeRecord.AssetPath = Record.AssetPath;
	RuntimeRecord.Status = FBlueprintHelperTaskRuntimePostOperationJson::StatusToString(Record.Status);
	RuntimeRecord.Reason = Record.Reason;
	RuntimeRecord.DurationMs = Record.DurationMs;
	return RuntimeRecord;
}

FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::
FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor(
	const FBlueprintHelperTaskRuntimeCommitService* InCommitService,
	FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace* InTimingTrace)
	: CommitService(InCommitService)
	, TimingTrace(InTimingTrace)
{
}

EBlueprintHelperTaskRuntimePipelineStage
FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::GetStage() const
{
	return EBlueprintHelperTaskRuntimePipelineStage::RunPostOperations;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::Execute(
	FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
{
	LastExecutionResult = FBlueprintHelperTaskRuntimePostOperationExecutionResult();
	if (Context.bDryRun)
	{
		return BlueprintHelperMakeTaskRuntimePipelineStageResult(TEXT("task_runtime_post_operations"));
	}

	const double PlanStageStart = TimingTrace
		? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
		: 0.0;
	const FBlueprintHelperTaskRuntimePostOperationPlan Plan =
		FBlueprintHelperTaskRuntimePostOperationPlanner::BuildPlan(Context.TaskPlan, Context.bDryRun);
	if (TimingTrace)
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			*TimingTrace,
			TEXT("main_thread_commit.post_operation_plan"),
			PlanStageStart);
	}

	if (!Plan.bHasTargetAssets)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("missing_target_assets_for_post_operation");
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = TEXT("TaskPlan execution_policy compile/save requires target_assets.");
		Error.Field = TEXT("task_plan.target_assets");
		LastExecutionResult.bOk = false;
		LastExecutionResult.FirstError = Error;
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("task_runtime_post_operations"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	const FBlueprintHelperTaskRuntimePostOperationExecutionResult CompileResult = ExecuteKind(
		Plan,
		EBlueprintHelperTaskRuntimePostOperationKind::Compile,
		TEXT("main_thread_commit.compile"),
		Context);
	if (!CompileResult.bOk)
	{
		LastExecutionResult = CompileResult;
		return MakePostOperationFailure(LastExecutionResult);
	}

	const FBlueprintHelperTaskRuntimePostOperationExecutionResult SaveResult = ExecuteKind(
		Plan,
		EBlueprintHelperTaskRuntimePostOperationKind::Save,
		TEXT("main_thread_commit.save"),
		Context);
	if (!SaveResult.bOk)
	{
		LastExecutionResult = SaveResult;
		return MakePostOperationFailure(LastExecutionResult);
	}

	LastExecutionResult = SaveResult;
	return BlueprintHelperMakeTaskRuntimePipelineStageResult(TEXT("task_runtime_post_operations"));
}

const FBlueprintHelperTaskRuntimePostOperationExecutionResult&
FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::GetLastExecutionResult() const
{
	return LastExecutionResult;
}

FBlueprintHelperTaskRuntimePostOperationExecutionResult
FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::ExecuteKind(
	const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
	EBlueprintHelperTaskRuntimePostOperationKind Kind,
	const FString& TimingName,
	FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
{
	const double StageStart = TimingTrace
		? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
		: 0.0;
	FBlueprintHelperTaskRuntimePostOperationStageExecutor Executor(Plan, Kind, CommitService);
	Executor.Execute(Context);
	if (TimingTrace)
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(*TimingTrace, TimingName, StageStart);
	}
	return Executor.GetExecutionResult();
}

FBlueprintHelperToolResultBase
FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor::MakePostOperationFailure(
	const FBlueprintHelperTaskRuntimePostOperationExecutionResult& ExecutionResult)
{
	FBlueprintHelperToolError Error;
	if (ExecutionResult.FirstError.IsSet())
	{
		Error = *ExecutionResult.FirstError;
	}
	else
	{
		Error.Code = TEXT("task_post_operation_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("TaskPlan post operation failed.");
	}
	return FBlueprintHelperToolResultBuilder::Failure(
		TEXT("task_runtime_post_operations"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		Error);
}
