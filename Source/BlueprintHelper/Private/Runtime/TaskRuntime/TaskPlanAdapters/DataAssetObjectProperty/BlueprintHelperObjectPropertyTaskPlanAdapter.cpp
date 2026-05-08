// BlueprintHelper TaskPlan adapter - UObject reflected property cluster.

#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

namespace
{
	FBlueprintHelperToolError MakeObjectPropertyTaskPlanError(
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

	FString ObjectPropertyBuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	FString ObjectPropertyBuildOpFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0].write.ops[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), *Suffix);
	}

	FString ObjectPropertyBuildSettingFieldPath(int32 SettingIndex, const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString::Printf(TEXT("task_plan.steps[0].write.ops[0].settings[%d]"), SettingIndex)
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].settings[%d].%s"), SettingIndex, *Suffix);
	}

	FString ObjectPropertyJsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("missing");
		}

		switch (Value->Type)
		{
		case EJson::None: return TEXT("missing");
		case EJson::Null: return TEXT("null");
		case EJson::String: return TEXT("string");
		case EJson::Number: return TEXT("number");
		case EJson::Boolean: return TEXT("bool");
		case EJson::Array: return TEXT("array");
		case EJson::Object: return TEXT("object");
		default: return TEXT("unknown");
		}
	}

	bool ObjectPropertyTryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		const TSharedPtr<FJsonValue>* FoundValue = Object.IsValid()
			? Object->Values.Find(FieldName)
			: nullptr;
		if (!FoundValue || !FoundValue->IsValid())
		{
			OutError = MakeObjectPropertyTaskPlanError(ErrorCode, ErrorMessage, FieldPath);
			return false;
		}
		if ((*FoundValue)->Type != EJson::String)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				ErrorCode,
				FString::Printf(
					TEXT("object_property field %s must be a string; actual type is %s."),
					FieldName,
					*ObjectPropertyJsonValueTypeToString(*FoundValue)),
				FieldPath);
			return false;
		}

		OutValue = (*FoundValue)->AsString();
		if (OutValue.IsEmpty())
		{
			OutError = MakeObjectPropertyTaskPlanError(
				ErrorCode,
				FString::Printf(TEXT("object_property field %s cannot be empty."), FieldName),
				FieldPath);
			return false;
		}
		return true;
	}

	bool ObjectPropertyTryRequireValueField(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldPath,
		const FString& ErrorCode,
		TSharedPtr<FJsonValue>& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Reset();
		const TSharedPtr<FJsonValue>* FoundValue = Object.IsValid()
			? Object->Values.Find(TEXT("value"))
			: nullptr;
		if (!FoundValue || !FoundValue->IsValid() ||
			(*FoundValue)->Type == EJson::None ||
			(*FoundValue)->Type == EJson::Null)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				ErrorCode,
				TEXT("object_property write setting requires a non-null value."),
				FieldPath);
			return false;
		}

		OutValue = *FoundValue;
		return true;
	}

	bool ObjectPropertyTryReadTaskPlanParts(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutStepId,
		FString& OutAssetPath,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		OutStepId.Empty();
		OutAssetPath.Empty();
		OutOpObject.Reset();

		if (!StepObject.IsValid())
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_step"),
				TEXT("object_property TaskPlan step must be an object."),
				ObjectPropertyBuildStepFieldPath(TEXT("")));
			return false;
		}

		StepObject->TryGetStringField(TEXT("step_id"), OutStepId);
		if (OutStepId.IsEmpty())
		{
			OutStepId = TEXT("step_001");
		}

		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("unsupported_object_property_operation_field"),
				TEXT("object_property IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				ObjectPropertyBuildStepFieldPath(TEXT("operation")));
			return false;
		}

		FString Capability;
		if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
			Capability != FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("unsupported_object_property_capability"),
				TEXT("object_property adapter requires capability=object_property."),
				ObjectPropertyBuildStepFieldPath(TEXT("capability")));
			return false;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_target"),
				TEXT("object_property TaskPlan step target object is required."),
				ObjectPropertyBuildStepFieldPath(TEXT("target")));
			return false;
		}

		if (!ObjectPropertyTryReadRequiredString(
			*TargetObjectPtr,
			TEXT("asset_path"),
			ObjectPropertyBuildStepFieldPath(TEXT("target.asset_path")),
			TEXT("invalid_object_property_target"),
			TEXT("object_property TaskPlan target requires asset_path."),
			OutAssetPath,
			OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_write"),
				TEXT("object_property TaskPlan step requires write object."),
				ObjectPropertyBuildStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperObjectPropertyTaskPlanAdapter::StrategyPropertyEdit)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("unsupported_object_property_strategy"),
				TEXT("object_property TaskPlan adapter currently supports property_edit strategy only."),
				ObjectPropertyBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray || OpsArray->Num() != 1)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_ops"),
				TEXT("object_property adapter currently lowers exactly one write.ops entry per TaskPlan step."),
				ObjectPropertyBuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid()
			? (*OpsArray)[0]->AsObject()
			: nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_op"),
				TEXT("object_property write.ops entry must be an object."),
				ObjectPropertyBuildOpFieldPath(TEXT("")));
			return false;
		}

		return true;
	}

	bool ObjectPropertyTryBuildSetPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		FString PropertyPath;
		if (!ObjectPropertyTryReadRequiredString(
			OpObject,
			TEXT("property_path"),
			ObjectPropertyBuildOpFieldPath(TEXT("property_path")),
			TEXT("invalid_object_property_setting"),
			TEXT("set_object_property requires property_path."),
			PropertyPath,
			OutError))
		{
			return false;
		}

		TSharedPtr<FJsonValue> Value;
		if (!ObjectPropertyTryRequireValueField(
			OpObject,
			ObjectPropertyBuildOpFieldPath(TEXT("value")),
			TEXT("invalid_object_property_setting"),
			Value,
			OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetStringField(TEXT("property_path"), PropertyPath);
		Payload->SetField(TEXT("value"), Value);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		OutPayload = Payload;
		return true;
	}

	bool ObjectPropertyTryBuildSetManyPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* SourceSettings = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetArrayField(TEXT("settings"), SourceSettings) ||
			!SourceSettings || SourceSettings->Num() == 0)
		{
			OutError = MakeObjectPropertyTaskPlanError(
				TEXT("invalid_object_property_settings"),
				TEXT("set_object_properties requires a non-empty settings array."),
				ObjectPropertyBuildOpFieldPath(TEXT("settings")));
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Settings;
		for (int32 Index = 0; Index < SourceSettings->Num(); ++Index)
		{
			TSharedPtr<FJsonObject> SettingObject = (*SourceSettings)[Index].IsValid()
				? (*SourceSettings)[Index]->AsObject()
				: nullptr;
			if (!SettingObject.IsValid())
			{
				OutError = MakeObjectPropertyTaskPlanError(
					TEXT("invalid_object_property_settings"),
					TEXT("object_property settings entries must be objects."),
					ObjectPropertyBuildSettingFieldPath(Index, TEXT("")));
				return false;
			}

			FString PropertyPath;
			if (!ObjectPropertyTryReadRequiredString(
				SettingObject,
				TEXT("property_path"),
				ObjectPropertyBuildSettingFieldPath(Index, TEXT("property_path")),
				TEXT("invalid_object_property_settings"),
				TEXT("object_property settings entries require property_path."),
				PropertyPath,
				OutError))
			{
				return false;
			}

			TSharedPtr<FJsonValue> Value;
			if (!ObjectPropertyTryRequireValueField(
				SettingObject,
				ObjectPropertyBuildSettingFieldPath(Index, TEXT("value")),
				TEXT("invalid_object_property_settings"),
				Value,
				OutError))
			{
				return false;
			}

			TSharedRef<FJsonObject> CopiedSetting = MakeShared<FJsonObject>();
			CopiedSetting->SetStringField(TEXT("property_path"), PropertyPath);
			CopiedSetting->SetField(TEXT("value"), Value);
			Settings.Add(MakeShared<FJsonValueObject>(CopiedSetting));
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetArrayField(TEXT("settings"), Settings);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		OutPayload = Payload;
		return true;
	}
}

bool FBlueprintHelperObjectPropertyTaskPlanAdapter::SupportsStep(const TSharedPtr<FJsonObject>& StepObject)
{
	FString Capability;
	return StepObject.IsValid() &&
		StepObject->TryGetStringField(TEXT("capability"), Capability) &&
		Capability == CapabilityObjectProperty;
}

bool FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperObjectPropertyTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperObjectPropertyTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	FString StepId;
	FString AssetPath;
	TSharedPtr<FJsonObject> OpObject;
	if (!ObjectPropertyTryReadTaskPlanParts(StepObject, StepId, AssetPath, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!ObjectPropertyTryReadRequiredString(
		OpObject,
		TEXT("op"),
		ObjectPropertyBuildOpFieldPath(TEXT("op")),
		TEXT("invalid_object_property_op"),
		TEXT("object_property op requires op."),
		OpName,
		OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	if (OpName == OpSetObjectProperty)
	{
		AdapterOperation = AdapterOperationSetObjectProperty;
		if (!ObjectPropertyTryBuildSetPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpSetObjectProperties)
	{
		AdapterOperation = AdapterOperationSetObjectProperties;
		if (!ObjectPropertyTryBuildSetManyPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName.StartsWith(TEXT("read_")))
	{
		OutError = MakeObjectPropertyTaskPlanError(
			TEXT("unsupported_object_property_read_op"),
			TEXT("object_property TaskRuntime write lowering does not execute read ops; use ReadSpec/read_context for reads."),
			ObjectPropertyBuildOpFieldPath(TEXT("op")));
		return false;
	}
	else
	{
		OutError = MakeObjectPropertyTaskPlanError(
			TEXT("unsupported_object_property_op"),
			TEXT("object_property adapter currently supports set_object_property and set_object_properties only."),
			ObjectPropertyBuildOpFieldPath(TEXT("op")));
		return false;
	}

	OutPayload.StepId = StepId;
	OutPayload.Capability = CapabilityObjectProperty;
	OutPayload.RuntimeOperation = RuntimeOperationObjectProperty;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}

bool FBlueprintHelperObjectPropertyTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	static_cast<void>(TaskPlan);
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
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
