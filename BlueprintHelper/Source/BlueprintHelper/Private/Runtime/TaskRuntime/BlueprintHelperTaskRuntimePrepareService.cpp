// BlueprintHelper TaskRuntime pure prepare service.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareService.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"

bool FBlueprintHelperTaskRuntimePrepareService::Prepare(
	const TSharedPtr<FJsonObject>& Payload,
	bool bDryRun,
	FBlueprintHelperTaskRuntimePreparedTaskRun& OutPreparedRun,
	FBlueprintHelperToolError& OutError) const
{
	OutPreparedRun = FBlueprintHelperTaskRuntimePreparedTaskRun();
	OutPreparedRun.bDryRun = bDryRun;

	if (!Payload.IsValid())
	{
		OutError = MakeTaskRuntimeError(
			TEXT("invalid_taskplan_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
		return false;
	}

	const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) ||
		!TaskPlanPtr ||
		!TaskPlanPtr->IsValid())
	{
		OutError = MakeTaskRuntimeError(
			TEXT("missing_task_plan"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload.task_plan is required."),
			TEXT("payload.task_plan"));
		return false;
	}
	OutPreparedRun.TaskPlan = *TaskPlanPtr;
	OutPreparedRun.TargetAssets = ReadTargetAssets(OutPreparedRun.TaskPlan);
	OutPreparedRun.DryRunPolicy =
		FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(OutPreparedRun.TaskPlan.ToSharedRef());

	if (bDryRun &&
		OutPreparedRun.DryRunPolicy.GetMode() == EBlueprintHelperTaskRuntimeDryRunMode::None)
	{
		OutError = MakeTaskRuntimeError(
			TEXT("dry_run_mode_none_requires_preview_token"),
			EBlueprintHelperToolStage::DryRun,
			TEXT("execution_policy.dry_run_mode=none is only allowed when AgentFace reuses a validated preview token and skips PreviewTaskPlan."),
			TEXT("task_plan.execution_policy.dry_run_mode"));
		return false;
	}
	OutPreparedRun.bQuickDryRun = bDryRun && OutPreparedRun.DryRunPolicy.ShouldRunQuickPreview();

	const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
	if (!OutPreparedRun.TaskPlan->TryGetArrayField(TEXT("steps"), StepsArray) ||
		!StepsArray ||
		StepsArray->Num() == 0)
	{
		OutError = MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_step_count"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime requires at least one TaskPlan step."),
			TEXT("task_plan.steps"));
		return false;
	}

	for (int32 StepIndex = 0; StepIndex < StepsArray->Num(); ++StepIndex)
	{
		const TSharedPtr<FJsonObject> StepObject =
			(*StepsArray)[StepIndex].IsValid()
				? (*StepsArray)[StepIndex]->AsObject()
				: nullptr;
		if (!StepObject.IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step must be an object."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex));
			return false;
		}

		FBlueprintHelperTaskRuntimePreparedStep PreparedStep;
		PreparedStep.StepIndex = StepIndex;
		PreparedStep.StepId = GetTaskPlanStepId(StepObject, StepIndex);
		PreparedStep.DependsOn = ReadStepDependsOn(StepObject);
		PreparedStep.StepObject = StepObject;

		FBlueprintHelperToolError LoweringError;
		if (!FBlueprintHelperTaskRuntimeClusterHub::TryLowerStep(
			OutPreparedRun.TaskPlan,
			StepObject,
			bDryRun,
			PreparedStep.LoweredStep,
			LoweringError))
		{
			NormalizeErrorField(LoweringError, StepIndex);
			OutError = LoweringError;
			return false;
		}

		if (!PreparedStep.LoweredStep.Payload.IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_lowered_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Task Runtime lowering did not produce a payload."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex));
			return false;
		}

		OutPreparedRun.Steps.Add(PreparedStep);
	}

	if (!bDryRun)
	{
		OutPreparedRun.TaskRunId = FString::Printf(
			TEXT("task_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPreparedRun.ArchiveSessionId = FString::Printf(
			TEXT("archive_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	return true;
}

FString FBlueprintHelperTaskRuntimePrepareService::GetTaskPlanStepId(
	const TSharedPtr<FJsonObject>& StepObject,
	int32 StepIndex)
{
	FString StepId;
	if (StepObject.IsValid() &&
		StepObject->TryGetStringField(TEXT("step_id"), StepId) &&
		!StepId.IsEmpty())
	{
		return StepId;
	}

	return FString::Printf(TEXT("step_%03d"), StepIndex + 1);
}

TArray<FString> FBlueprintHelperTaskRuntimePrepareService::ReadStepDependsOn(
	const TSharedPtr<FJsonObject>& StepObject)
{
	TArray<FString> DependsOn;
	const TArray<TSharedPtr<FJsonValue>>* DependsOnValues = nullptr;
	if (!StepObject.IsValid() ||
		!StepObject->TryGetArrayField(TEXT("depends_on"), DependsOnValues) ||
		!DependsOnValues)
	{
		return DependsOn;
	}

	for (const TSharedPtr<FJsonValue>& DependsOnValue : *DependsOnValues)
	{
		if (!DependsOnValue.IsValid())
		{
			continue;
		}

		const FString StepId = DependsOnValue->AsString();
		if (!StepId.IsEmpty())
		{
			DependsOn.Add(StepId);
		}
	}

	return DependsOn;
}

TArray<FString> FBlueprintHelperTaskRuntimePrepareService::ReadTargetAssets(
	const TSharedPtr<FJsonObject>& TaskPlan)
{
	TArray<FString> Assets;
	const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
	if (!TaskPlan.IsValid() ||
		!TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) ||
		!TargetAssets)
	{
		return Assets;
	}

	for (const TSharedPtr<FJsonValue>& AssetValue : *TargetAssets)
	{
		if (!AssetValue.IsValid() || AssetValue->Type != EJson::String)
		{
			continue;
		}
		const FString AssetPath = AssetValue->AsString();
		if (!AssetPath.IsEmpty())
		{
			Assets.AddUnique(AssetPath);
		}
	}
	return Assets;
}

FBlueprintHelperToolError FBlueprintHelperTaskRuntimePrepareService::MakeTaskRuntimeError(
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
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

void FBlueprintHelperTaskRuntimePrepareService::NormalizeErrorField(
	FBlueprintHelperToolError& Error,
	int32 StepIndex)
{
	if (StepIndex != 0 && Error.Field.Contains(TEXT("task_plan.steps[0]")))
	{
		Error.Field = Error.Field.Replace(
			TEXT("task_plan.steps[0]"),
			*FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex));
	}
}
