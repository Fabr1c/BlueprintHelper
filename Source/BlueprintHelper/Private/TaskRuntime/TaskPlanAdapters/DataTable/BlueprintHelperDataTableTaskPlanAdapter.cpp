// BlueprintHelper Service Layer - DataTable TaskPlan lowering adapter

#include "TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"

namespace
{
	FBlueprintHelperToolError MakeDataTableTaskPlanError(
		const FString& Code,
		const FString& Message,
		const FString& Field)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	FString BuildOpFieldPath(int32 OpIndex, const FString& Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, *Field);
	}

	bool TryReadStepTargetAssetPath(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("invalid_data_table_target"),
				TEXT("data_table TaskPlan step target object is required."),
				TEXT("task_plan.steps[0].target"));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("invalid_data_table_target"),
				TEXT("data_table TaskPlan step target requires asset_path."),
				TEXT("task_plan.steps[0].target.asset_path"));
			return false;
		}

		return true;
	}

	bool TryReadSingleDataTableOp(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("invalid_data_table_write"),
				TEXT("data_table TaskPlan step requires write object."),
				TEXT("task_plan.steps[0].write"));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperDataTableTaskPlanAdapter::StrategyRowEdit)
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("unsupported_data_table_strategy"),
				TEXT("DataTable TaskPlan adapter currently supports row_edit only."),
				TEXT("task_plan.steps[0].write.strategy"));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray || OpsArray->Num() == 0)
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("invalid_data_table_ops"),
				TEXT("data_table TaskPlan step requires one write op."),
				TEXT("task_plan.steps[0].write.ops"));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("unsupported_data_table_batch_ops"),
				TEXT("DataTable TaskPlan adapter is limited to one service-backed row op until batch service support exists."),
				TEXT("task_plan.steps[0].write.ops"));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid() ? (*OpsArray)[0]->AsObject() : nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeDataTableTaskPlanError(
				TEXT("invalid_data_table_op"),
				TEXT("data_table write op must be an object."),
				TEXT("task_plan.steps[0].write.ops[0]"));
			return false;
		}

		return true;
	}

	bool TryRequireRowName(
		const TSharedPtr<FJsonObject>& OpObject,
		const FString& ErrorCode,
		FString& OutRowName,
		FBlueprintHelperToolError& OutError)
	{
		if (!OpObject.IsValid() || !OpObject->TryGetStringField(TEXT("row_name"), OutRowName) || OutRowName.IsEmpty())
		{
			OutError = MakeDataTableTaskPlanError(
				ErrorCode,
				TEXT("DataTable row op requires row_name."),
				BuildOpFieldPath(0, TEXT("row_name")));
			return false;
		}

		return true;
	}

	bool TryRequireFieldsObject(
		const TSharedPtr<FJsonObject>& OpObject,
		const FString& ErrorCode,
		const TSharedPtr<FJsonObject>*& OutFieldsObjectPtr,
		FBlueprintHelperToolError& OutError)
	{
		OutFieldsObjectPtr = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetObjectField(TEXT("fields"), OutFieldsObjectPtr) ||
			!OutFieldsObjectPtr || !OutFieldsObjectPtr->IsValid() ||
			(*OutFieldsObjectPtr)->Values.Num() == 0)
		{
			OutError = MakeDataTableTaskPlanError(
				ErrorCode,
				TEXT("DataTable update row op requires a non-empty fields object."),
				BuildOpFieldPath(0, TEXT("fields")));
			return false;
		}

		return true;
	}

	void CopyFieldsObjectIfPresent(
		const TSharedPtr<FJsonObject>& OpObject,
		const TSharedRef<FJsonObject>& Payload)
	{
		const TSharedPtr<FJsonObject>* FieldsObjectPtr = nullptr;
		if (OpObject.IsValid() &&
			OpObject->TryGetObjectField(TEXT("fields"), FieldsObjectPtr) &&
			FieldsObjectPtr && FieldsObjectPtr->IsValid())
		{
			Payload->SetObjectField(TEXT("fields"), *FieldsObjectPtr);
		}
	}
}

bool FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperDataTableTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperDataTableTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = MakeDataTableTaskPlanError(
			TEXT("invalid_data_table_step"),
			TEXT("DataTable TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutPayload.StepId);
	if (OutPayload.StepId.IsEmpty())
	{
		OutPayload.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (Capability != CapabilityDataTable)
	{
		OutError = MakeDataTableTaskPlanError(
			TEXT("unsupported_data_table_capability"),
			TEXT("DataTable adapter requires capability=data_table."),
			TEXT("task_plan.steps[0].capability"));
		return false;
	}

	FString LegacyOperation;
	if (StepObject->TryGetStringField(TEXT("operation"), LegacyOperation))
	{
		OutError = MakeDataTableTaskPlanError(
			TEXT("unsupported_data_table_operation_field"),
			TEXT("DataTable IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			TEXT("task_plan.steps[0].operation"));
		return false;
	}

	FString AssetPath;
	if (!TryReadStepTargetAssetPath(StepObject, AssetPath, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> OpObject;
	if (!TryReadSingleDataTableOp(StepObject, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	OpObject->TryGetStringField(TEXT("op"), OpName);

	FString AdapterOperation;
	FString OpErrorCode;
	if (OpName == OpAddRow)
	{
		AdapterOperation = AdapterOperationAddRow;
		OpErrorCode = TEXT("invalid_data_table_add_row_op");
	}
	else if (OpName == OpUpdateRow)
	{
		AdapterOperation = AdapterOperationUpdateRow;
		OpErrorCode = TEXT("invalid_data_table_update_row_op");
	}
	else if (OpName == OpDeleteRow)
	{
		AdapterOperation = AdapterOperationDeleteRow;
		OpErrorCode = TEXT("invalid_data_table_delete_row_op");
	}
	else
	{
		OutError = MakeDataTableTaskPlanError(
			TEXT("unsupported_data_table_op"),
			TEXT("DataTable TaskPlan adapter supports add_row, update_row, and delete_row only."),
			BuildOpFieldPath(0, TEXT("op")));
		return false;
	}

	FString RowName;
	if (!TryRequireRowName(OpObject, OpErrorCode, RowName, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RequiredFieldsObjectPtr = nullptr;
	if (OpName == OpUpdateRow &&
		!TryRequireFieldsObject(OpObject, OpErrorCode, RequiredFieldsObjectPtr, OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetStringField(TEXT("row_name"), RowName);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (OpName == OpUpdateRow)
	{
		Payload->SetObjectField(TEXT("fields"), *RequiredFieldsObjectPtr);
	}
	else if (OpName == OpAddRow)
	{
		CopyFieldsObjectIfPresent(OpObject, Payload);
	}

	OutPayload.Capability = CapabilityDataTable;
	OutPayload.RuntimeOperation = RuntimeOperationDataTable;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = false;
	return true;
}

bool FBlueprintHelperDataTableTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	(void)TaskPlan;
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	if (!TryBuildPayloadFromTaskPlanStep(StepObject, bDryRun, BuiltPayload, OutError))
	{
		return false;
	}

	OutLoweredStep.StepId = BuiltPayload.StepId;
	OutLoweredStep.Capability = BuiltPayload.Capability;
	OutLoweredStep.RuntimeOperation = BuiltPayload.RuntimeOperation;
	OutLoweredStep.AdapterOperation = BuiltPayload.AdapterOperation;
	OutLoweredStep.Payload = BuiltPayload.Payload;
	OutLoweredStep.bAdapterDryRunSupported = BuiltPayload.bAdapterDryRunSupported;
	return true;
}
