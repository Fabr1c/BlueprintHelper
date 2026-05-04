// BlueprintHelper Service Layer - TaskPlan runtime executor

#include "Services/BlueprintHelperTaskRuntimeService.h"
#include "Services/BlueprintHelperAppendBlueprintGraphService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	FBlueprintHelperToolError MakeTaskRuntimeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	FBlueprintHelperToolResultBase MakeFailure(
		const FString& Operation,
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeTaskRuntimeError(Code, Stage, Message, Field));
	}

	TArray<TSharedPtr<FJsonValue>> CopyArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(FieldName, Array) && Array)
		{
			return *Array;
		}
		return {};
	}

	bool HasExecutionPolicyValidationFields(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if (!TaskPlan.IsValid() ||
			!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
			!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
		{
			return false;
		}

		bool bIgnored = false;
		return (*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bIgnored) ||
			(*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bIgnored);
	}

	TSharedRef<FJsonObject> BuildAppendPayload(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 target 对象。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return MakeShared<FJsonObject>();
		}
		if (!StepObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) ||
			!ArgsObjectPtr || !ArgsObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 args 对象。");
			OutErrorField = TEXT("task_plan.steps[0].args");
			return MakeShared<FJsonObject>();
		}

		FString AssetPath;
		FString GraphName;
		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath);
		(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), GraphName);
		if (AssetPath.IsEmpty() || GraphName.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_taskplan_step_target");
			OutErrorMessage = TEXT("TaskPlan step target 需要 asset_path 和 graph。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> AppendPayload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> AppendTarget = MakeShared<FJsonObject>();
		AppendTarget->SetStringField(TEXT("asset_path"), AssetPath);
		AppendTarget->SetStringField(TEXT("graph"), GraphName);
		AppendPayload->SetObjectField(TEXT("target"), AppendTarget);
		AppendPayload->SetBoolField(TEXT("dry_run"), bDryRun);

		FString FeatureName;
		if ((*ArgsObjectPtr)->TryGetStringField(TEXT("feature_name"), FeatureName) && !FeatureName.IsEmpty())
		{
			AppendPayload->SetStringField(TEXT("feature_name"), FeatureName);
		}

		AppendPayload->SetArrayField(TEXT("nodes"), CopyArrayField(*ArgsObjectPtr, TEXT("nodes")));
		AppendPayload->SetArrayField(TEXT("links"), CopyArrayField(*ArgsObjectPtr, TEXT("links")));
		return AppendPayload;
	}

	TSharedRef<FJsonObject> MakeStepResultJson(
		const FString& StepId,
		const FString& Operation,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		TSharedRef<FJsonObject> StepJson = MakeShared<FJsonObject>();
		StepJson->SetStringField(TEXT("step_id"), StepId);
		StepJson->SetStringField(TEXT("operation"), Operation);
		StepJson->SetStringField(TEXT("status"), ToolStatusToString(StepResult.Status));
		StepJson->SetObjectField(TEXT("result"), StepResult.ToJson());
		return StepJson;
	}

	TSharedRef<FJsonObject> MakeRuntimeData(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& TaskRunId,
		const FString& StepId,
		const FString& StepOperation,
		const FBlueprintHelperToolResultBase& StepResult,
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
		Steps.Add(MakeShared<FJsonValueObject>(MakeStepResultJson(StepId, StepOperation, StepResult)));
		Data->SetArrayField(TEXT("steps"), Steps);

		if (bDryRun && StepResult.Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
			if (StepResult.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) &&
				DryRunObject && DryRunObject->IsValid())
			{
				Data->SetObjectField(TEXT("dry_run"), *DryRunObject);
			}
		}

		return Data;
	}

	TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& StepId,
		const FString& StepOperation,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
		Journal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
		Journal->SetStringField(TEXT("task_run_id"), TaskRunId);
		Journal->SetStringField(TEXT("status"), StepResult.bOk ? TEXT("completed") : TEXT("failed"));

		if (TaskPlan.IsValid())
		{
			FString TaskType;
			if (TaskPlan->TryGetStringField(TEXT("task_type"), TaskType))
			{
				Journal->SetStringField(TEXT("task_type"), TaskType);
			}
			FString FeatureName;
			if (TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName))
			{
				Journal->SetStringField(TEXT("feature_name"), FeatureName);
			}
			const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
			if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
			{
				Journal->SetArrayField(TEXT("target_assets"), *TargetAssets);
			}
		}

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeStepResultJson(StepId, StepOperation, StepResult)));
		Journal->SetArrayField(TEXT("steps"), Steps);
		return Journal;
	}
}

FBlueprintHelperTaskRuntimeService::FBlueprintHelperTaskRuntimeService(
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService)
	: AppendGraphService(InAppendGraphService)
{
}

FBlueprintHelperValidationSummary FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperValidationSummary& BaseValidation)
{
	FBlueprintHelperValidationSummary RuntimeValidation = BaseValidation;

	const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
	if (!TaskPlan.IsValid() ||
		!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
		!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
	{
		return RuntimeValidation;
	}

	bool bShouldCompile = false;
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bShouldCompile))
	{
		RuntimeValidation.bShouldCompile = bShouldCompile;
	}

	bool bShouldSave = false;
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bShouldSave))
	{
		RuntimeValidation.bShouldSave = bShouldSave;
	}

	return RuntimeValidation;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::PreviewTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	return RunTaskPlan(Payload, true);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::ExecuteTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	return RunTaskPlan(Payload, false);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::GetTaskRunJournal(
	const FString& TaskRunId) const
{
	const TSharedPtr<FJsonObject>* FoundJournal = TaskRunJournals.Find(TaskRunId);
	if (!FoundJournal || !FoundJournal->IsValid())
	{
		return MakeFailure(
			TEXT("get_task_run_journal"),
			TEXT("task_run_journal_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("TaskRunJournal not found: %s"), *TaskRunId),
			TEXT("task_run_id"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_task_run_journal"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = *FoundJournal;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::RunTaskPlan(
	const TSharedPtr<FJsonObject>& Payload,
	bool bDryRun) const
{
	const FString RuntimeOperation = bDryRun ? TEXT("preview_task_plan") : TEXT("execute_task_plan");
	if (!Payload.IsValid())
	{
		return MakeFailure(RuntimeOperation, TEXT("invalid_taskplan_payload"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload 缺失。"), TEXT("payload"));
	}

	const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) || !TaskPlanPtr || !TaskPlanPtr->IsValid())
	{
		return MakeFailure(RuntimeOperation, TEXT("missing_task_plan"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload.task_plan 缺失。"), TEXT("payload.task_plan"));
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
	if (!(*TaskPlanPtr)->TryGetArrayField(TEXT("steps"), StepsArray) || !StepsArray || StepsArray->Num() != 1)
	{
		return MakeFailure(RuntimeOperation, TEXT("unsupported_taskplan_step_count"),
			EBlueprintHelperToolStage::ParseInput, TEXT("第一版 Task Runtime 只支持一个 TaskPlan step。"),
			TEXT("task_plan.steps"));
	}

	const TSharedPtr<FJsonObject> StepObject = (*StepsArray)[0].IsValid() ? (*StepsArray)[0]->AsObject() : nullptr;
	if (!StepObject.IsValid())
	{
		return MakeFailure(RuntimeOperation, TEXT("invalid_taskplan_step"),
			EBlueprintHelperToolStage::ParseInput, TEXT("task_plan.steps[0] 必须是对象。"),
			TEXT("task_plan.steps[0]"));
	}

	FString StepOperation;
	StepObject->TryGetStringField(TEXT("operation"), StepOperation);
	if (StepOperation != TEXT("append_blueprint_graph"))
	{
		return MakeFailure(RuntimeOperation, TEXT("unsupported_taskplan_operation"),
			EBlueprintHelperToolStage::ParseInput, TEXT("第一版 Task Runtime 只支持 append_blueprint_graph step。"),
			TEXT("task_plan.steps[0].operation"));
	}

	FString AppendPayloadErrorCode;
	FString AppendPayloadErrorMessage;
	FString AppendPayloadErrorField;
	TSharedRef<FJsonObject> AppendPayload = BuildAppendPayload(
		StepObject,
		bDryRun,
		AppendPayloadErrorCode,
		AppendPayloadErrorMessage,
		AppendPayloadErrorField);
	if (!AppendPayloadErrorCode.IsEmpty())
	{
		return MakeFailure(RuntimeOperation, AppendPayloadErrorCode,
			EBlueprintHelperToolStage::ParseInput, AppendPayloadErrorMessage, AppendPayloadErrorField);
	}

	FString StepId;
	StepObject->TryGetStringField(TEXT("step_id"), StepId);
	if (StepId.IsEmpty())
	{
		StepId = TEXT("step_001");
	}

	const FBlueprintHelperToolResultBase StepResult = AppendGraphService.Execute(AppendPayload);
	FBlueprintHelperToolResultBase RuntimeResult = StepResult.bOk
		? (bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId())
			: FBlueprintHelperToolResultBuilder::Applied(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId()))
		: FBlueprintHelperToolResultBuilder::Failure(
			RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			StepResult.Error.IsSet()
				? *StepResult.Error
				: MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step 执行失败。")));

	if (StepResult.Target.IsSet())
	{
		RuntimeResult.Target = StepResult.Target;
	}

	if (StepResult.Validation.IsSet() || HasExecutionPolicyValidationFields(*TaskPlanPtr))
	{
		FBlueprintHelperValidationSummary BaseValidation;
		if (StepResult.Validation.IsSet())
		{
			BaseValidation = *StepResult.Validation;
		}
		RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
	}

	const FString TaskRunId = bDryRun
		? TEXT("")
		: FString::Printf(TEXT("task_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	RuntimeResult.Data = MakeRuntimeData(*TaskPlanPtr, TaskRunId, StepId, StepOperation, StepResult, bDryRun);

	if (!bDryRun && !TaskRunId.IsEmpty())
	{
		TaskRunJournals.Add(TaskRunId, MakeTaskRunJournal(TaskRunId, *TaskPlanPtr, StepId, StepOperation, StepResult));
	}

	return RuntimeResult;
}
