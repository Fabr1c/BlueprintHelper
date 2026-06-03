// BlueprintHelper TaskPlan adapter - Blueprint Class Settings capability.

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils
{
public:
	static FString ClassSettingsStepFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].%s"), Field);
	}

	static FString ClassSettingsOpFieldPath(int32 OpIndex, const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, Field);
	}

	static FBlueprintHelperToolError MakeClassSettingsAdapterError(
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

	static bool ClassSettingsTryReadTargetAssetPath(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_taskplan_step_target"),
				TEXT("blueprint_class_settings TaskPlan step requires target object."),
				ClassSettingsStepFieldPath(TEXT("target")));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_taskplan_step_target"),
				TEXT("blueprint_class_settings TaskPlan step target requires asset_path."),
				ClassSettingsStepFieldPath(TEXT("target.asset_path")));
			return false;
		}

		return true;
	}

	static bool ClassSettingsTryReadWriteObject(
		const TSharedPtr<FJsonObject>& StepObject,
		const TSharedPtr<FJsonObject>*& OutWriteObjectPtr,
		FBlueprintHelperToolError& OutError)
	{
		OutWriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), OutWriteObjectPtr) ||
			!OutWriteObjectPtr || !OutWriteObjectPtr->IsValid())
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_write"),
				TEXT("blueprint_class_settings TaskPlan step requires write object."),
				ClassSettingsStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*OutWriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperClassSettingsTaskPlanAdapter::StrategyName)
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("unsupported_class_settings_strategy"),
				TEXT("blueprint_class_settings TaskPlan step supports class_settings strategy only."),
				ClassSettingsStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		return true;
	}

	static bool ClassSettingsTryReadSingleOp(
		const TSharedPtr<FJsonObject>& WriteObject,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!WriteObject.IsValid() ||
			!WriteObject->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray)
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_ops"),
				TEXT("blueprint_class_settings TaskPlan step requires write.ops array."),
				ClassSettingsStepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_ops"),
				TEXT("blueprint_class_settings TaskPlan step currently supports exactly one batch op."),
				ClassSettingsStepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid() ? (*OpsArray)[0]->AsObject() : nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_op"),
				TEXT("blueprint_class_settings op must be an object."),
				TEXT("task_plan.steps[0].write.ops[0]"));
			return false;
		}

		return true;
	}

	static bool ClassSettingsTryCopyStringArrayField(
		const TSharedPtr<FJsonObject>& OpObject,
		const TCHAR* FieldName,
		TArray<TSharedPtr<FJsonValue>>& OutArray,
		FBlueprintHelperToolError& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* SourceArray = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetArrayField(FieldName, SourceArray) ||
			!SourceArray)
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_op"),
				TEXT("Class Settings interface op requires interface_paths array."),
				ClassSettingsOpFieldPath(0, FieldName));
			return false;
		}

		for (int32 Index = 0; Index < SourceArray->Num(); ++Index)
		{
			FString Item;
			if (!(*SourceArray)[Index].IsValid() || !(*SourceArray)[Index]->TryGetString(Item))
			{
				OutError = MakeClassSettingsAdapterError(
					TEXT("invalid_class_settings_op"),
					TEXT("interface_paths entries must be strings."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s[%d]"), FieldName, Index));
				return false;
			}

			OutArray.Add(MakeShared<FJsonValueString>(Item));
		}

		return true;
	}

	static bool ClassSettingsTryCopySettingsArrayField(
		const TSharedPtr<FJsonObject>& OpObject,
		TArray<TSharedPtr<FJsonValue>>& OutArray,
		FBlueprintHelperToolError& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* SourceArray = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetArrayField(TEXT("settings"), SourceArray) ||
			!SourceArray)
		{
			OutError = MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_op"),
				TEXT("set_class_default_properties requires settings array."),
				ClassSettingsOpFieldPath(0, TEXT("settings")));
			return false;
		}

		for (int32 Index = 0; Index < SourceArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> SettingObject =
				(*SourceArray)[Index].IsValid()
					? (*SourceArray)[Index]->AsObject()
					: nullptr;
			if (!SettingObject.IsValid())
			{
				OutError = MakeClassSettingsAdapterError(
					TEXT("invalid_class_settings_op"),
					TEXT("settings entries must be objects."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[0].settings[%d]"), Index));
				return false;
			}

			FString PropertyPath;
			if (!SettingObject->TryGetStringField(TEXT("property_path"), PropertyPath) || PropertyPath.IsEmpty())
			{
				OutError = MakeClassSettingsAdapterError(
					TEXT("invalid_class_settings_op"),
					TEXT("settings entries require property_path."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[0].settings[%d].property_path"), Index));
				return false;
			}

			if (!SettingObject->HasField(TEXT("value")))
			{
				OutError = MakeClassSettingsAdapterError(
					TEXT("invalid_class_settings_op"),
					TEXT("settings entries require value."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[0].settings[%d].value"), Index));
				return false;
			}

			TSharedRef<FJsonObject> CopiedSetting = MakeShared<FJsonObject>();
			for (const auto& Field : SettingObject->Values)
			{
				CopiedSetting->SetField(FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key), Field.Value);
			}
			OutArray.Add(MakeShared<FJsonValueObject>(CopiedSetting));
		}

		return true;
	}

};

bool FBlueprintHelperClassSettingsTaskPlanAdapter::IsSupportedCapability(const FString& Capability)
{
	return Capability == CapabilityName;
}

bool FBlueprintHelperClassSettingsTaskPlanAdapter::IsSupportedOp(const FString& OpName)
{
	return OpName == AddImplementedInterfacesOp ||
		OpName == RemoveImplementedInterfacesOp ||
		OpName == SetClassDefaultPropertiesOp ||
		OpName == ReparentBlueprintOp;
}

bool FBlueprintHelperClassSettingsTaskPlanAdapter::IsParentClassOp(const FString& OpName)
{
	return OpName == TEXT("set_parent_class") ||
		OpName == TEXT("blueprint_reparent");
}

bool FBlueprintHelperClassSettingsTaskPlanAdapter::TryBuildAdapterPayload(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FString& OutAdapterOperation,
	FBlueprintHelperToolError& OutError)
{
	OutPayload.Reset();
	OutAdapterOperation.Reset();
	OutError = FBlueprintHelperToolError();

	FString AssetPath;
	if (!FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsTryReadTargetAssetPath(StepObject, AssetPath, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
	if (!FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsTryReadWriteObject(StepObject, WriteObjectPtr, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> OpObject;
	if (!FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsTryReadSingleOp(*WriteObjectPtr, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("invalid_class_settings_op"),
			TEXT("blueprint_class_settings op requires op name."),
			FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsOpFieldPath(0, TEXT("op")));
		return false;
	}

	if (IsParentClassOp(OpName))
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("unsupported_class_settings_parent_class_op"),
			TEXT("Parent class changes are not supported by blueprint_class_settings."),
			FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsOpFieldPath(0, TEXT("op")));
		return false;
	}

	if (!IsSupportedOp(OpName))
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("unsupported_class_settings_op"),
			TEXT("Unsupported blueprint_class_settings op."),
			FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsOpFieldPath(0, TEXT("op")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (OpName == AddImplementedInterfacesOp || OpName == RemoveImplementedInterfacesOp)
	{
		TArray<TSharedPtr<FJsonValue>> InterfacePaths;
		if (!FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsTryCopyStringArrayField(OpObject, TEXT("interface_paths"), InterfacePaths, OutError))
		{
			return false;
		}
		Payload->SetArrayField(TEXT("interface_paths"), InterfacePaths);
	}
	else if (OpName == SetClassDefaultPropertiesOp)
	{
		TArray<TSharedPtr<FJsonValue>> Settings;
		if (!FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsTryCopySettingsArrayField(OpObject, Settings, OutError))
		{
			return false;
		}
		Payload->SetArrayField(TEXT("settings"), Settings);
	}
	else if (OpName == ReparentBlueprintOp)
	{
		FString NewParentClass;
		if (!OpObject->TryGetStringField(TEXT("new_parent_class"), NewParentClass) || NewParentClass.IsEmpty())
		{
			OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
				TEXT("invalid_class_settings_op"),
				TEXT("reparent_blueprint requires new_parent_class."),
				FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsOpFieldPath(0, TEXT("new_parent_class")));
			return false;
		}
		Payload->SetStringField(TEXT("new_parent_class"), NewParentClass);
	}

	OutAdapterOperation = OpName;
	OutPayload = Payload;
	return true;
}

bool FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	static_cast<void>(TaskPlan);
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("invalid_taskplan_step"),
			TEXT("TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutLoweredStep.StepId);
	if (OutLoweredStep.StepId.IsEmpty())
	{
		OutLoweredStep.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (!IsSupportedCapability(Capability))
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("unsupported_taskplan_capability"),
			TEXT("Class Settings adapter supports blueprint_class_settings capability only."),
			FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsStepFieldPath(TEXT("capability")));
		return false;
	}

	FString AdapterOperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperationField))
	{
		OutError = FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::MakeClassSettingsAdapterError(
			TEXT("unsupported_class_settings_operation_field"),
			TEXT("blueprint_class_settings IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			FBlueprintHelperClassSettingsTaskPlanAdapterLocalUtils::ClassSettingsStepFieldPath(TEXT("operation")));
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	if (!TryBuildAdapterPayload(StepObject, bDryRun, Payload, AdapterOperation, OutError))
	{
		return false;
	}

	OutLoweredStep.Capability = CapabilityName;
	OutLoweredStep.RuntimeOperation = RuntimeOperationName;
	OutLoweredStep.AdapterOperation = AdapterOperation;
	OutLoweredStep.Payload = Payload;
	OutLoweredStep.bAdapterDryRunSupported = true;
	return true;
}
