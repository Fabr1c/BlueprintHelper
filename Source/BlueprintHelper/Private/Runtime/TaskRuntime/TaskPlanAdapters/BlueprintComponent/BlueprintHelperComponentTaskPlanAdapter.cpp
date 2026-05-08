// BlueprintHelper TaskPlan adapter - Blueprint Component cluster.

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent = TEXT("blueprint_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree = TEXT("component_tree");

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::RuntimeOperationBlueprintComponent = TEXT("blueprint_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent = TEXT("add_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties = TEXT("set_component_properties");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent = TEXT("remove_component");

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent = TEXT("add_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpSetComponentProperties = TEXT("set_component_properties");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpRemoveComponent = TEXT("remove_component");

namespace
{
	FString JsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	FBlueprintHelperToolError MakeComponentTaskPlanError(
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

	FString BuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	FString BuildOpFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0].write.ops[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), *Suffix);
	}

	bool TryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		const TSharedPtr<FJsonValue>* FoundValue = Object.IsValid()
			? Object->Values.Find(FieldName)
			: nullptr;
		if (!FoundValue || !FoundValue->IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(TEXT("Blueprint component op requires %s."), FieldName),
				FieldPath);
			return false;
		}
		if ((*FoundValue)->Type != EJson::String)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(TEXT("Blueprint component op %s must be a string."), FieldName),
				FieldPath);
			return false;
		}

		OutValue = (*FoundValue)->AsString();
		if (OutValue.IsEmpty())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(TEXT("Blueprint component op %s cannot be empty."), FieldName),
				FieldPath);
			return false;
		}
		return true;
	}

	bool TryCopyOptionalString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue>* FoundValue = Source.IsValid()
			? Source->Values.Find(FieldName)
			: nullptr;
		if (!FoundValue)
		{
			return true;
		}
		if (!FoundValue->IsValid() || (*FoundValue)->Type != EJson::String)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(
					TEXT("Blueprint component op %s must be a string when present; actual type is %s."),
					FieldName,
					*JsonValueTypeToString(FoundValue ? *FoundValue : TSharedPtr<FJsonValue>())),
				FieldPath);
			return false;
		}

		Destination->SetStringField(FieldName, (*FoundValue)->AsString());
		return true;
	}

	bool TryValidateStringEnum(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TCHAR* AllowedValueA,
		const TCHAR* AllowedValueB,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!Source.IsValid() || !Source->TryGetStringField(FieldName, Value))
		{
			return true;
		}
		if (Value != AllowedValueA && Value != AllowedValueB)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_op_field"),
				FString::Printf(TEXT("Unsupported blueprint component %s value: %s."), FieldName, *Value),
				FieldPath);
			return false;
		}
		return true;
	}

	bool TryCopyStringField(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!TryReadRequiredString(Source, FieldName, FieldPath, Value, OutError))
		{
			return false;
		}
		Destination->SetStringField(FieldName, Value);
		return true;
	}

	bool TryReadComponentTaskPlanParts(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		if (!StepObject.IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_taskplan_step"),
				TEXT("TaskPlan step must be an object."),
				BuildStepFieldPath(TEXT("")));
			return false;
		}

		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_operation_field"),
				TEXT("Blueprint component TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				BuildStepFieldPath(TEXT("operation")));
			return false;
		}

		FString Capability;
		if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
			Capability != FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_capability"),
				TEXT("Blueprint component adapter only supports capability blueprint_component."),
				BuildStepFieldPath(TEXT("capability")));
			return false;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_target"),
				TEXT("Blueprint component TaskPlan step target object is required."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) ||
			OutAssetPath.IsEmpty())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_target"),
				TEXT("Blueprint component TaskPlan step target requires asset_path."),
				BuildStepFieldPath(TEXT("target.asset_path")));
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_write"),
				TEXT("blueprint_component TaskPlan step requires write object."),
				BuildStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_strategy"),
				TEXT("Blueprint component adapter currently supports component_tree strategy only."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray || OpsArray->Num() != 1)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_ops"),
				TEXT("Blueprint component adapter currently lowers exactly one write.ops entry per TaskPlan step."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid()
			? (*OpsArray)[0]->AsObject()
			: nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				TEXT("Blueprint component write.ops entry must be an object."),
				BuildOpFieldPath(TEXT("")));
			return false;
		}

		return true;
	}

	bool TryValidateSettingsArray(
		const TSharedPtr<FJsonObject>& OpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Settings = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetArrayField(TEXT("settings"), Settings) ||
			!Settings || Settings->Num() == 0)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				TEXT("set_component_properties requires a non-empty settings array."),
				BuildOpFieldPath(TEXT("settings")));
			return false;
		}

		for (int32 Index = 0; Index < Settings->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> SettingObject = (*Settings)[Index].IsValid()
				? (*Settings)[Index]->AsObject()
				: nullptr;
			const FString SettingPath = FString::Printf(
				TEXT("task_plan.steps[0].write.ops[0].settings[%d]"),
				Index);
			if (!SettingObject.IsValid())
			{
				OutError = MakeComponentTaskPlanError(
					TEXT("invalid_blueprint_component_property_setting"),
					TEXT("Component property setting must be an object."),
					SettingPath);
				return false;
			}

			FString PropertyPath;
			if (!TryReadRequiredString(
				SettingObject,
				TEXT("property_path"),
				SettingPath + TEXT(".property_path"),
				PropertyPath,
				OutError))
			{
				OutError.Code = TEXT("invalid_blueprint_component_property_setting");
				return false;
			}

			if (!SettingObject->Values.Contains(TEXT("value")))
			{
				OutError = MakeComponentTaskPlanError(
					TEXT("invalid_blueprint_component_property_setting"),
					TEXT("Component property setting requires value."),
					SettingPath + TEXT(".value"));
				return false;
			}
		}

		return true;
	}

	bool TryBuildAddComponentPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyStringField(OpObject, TEXT("component_name"), BuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!TryCopyStringField(OpObject, TEXT("component_class"), BuildOpFieldPath(TEXT("component_class")), Payload, OutError))
		{
			return false;
		}

		if (!TryValidateStringEnum(
				OpObject,
				TEXT("attach_rule"),
				BuildOpFieldPath(TEXT("attach_rule")),
				TEXT("keep_relative"),
				TEXT("snap_to_target"),
				OutError) ||
			!TryValidateStringEnum(
				OpObject,
				TEXT("name_collision_policy"),
				BuildOpFieldPath(TEXT("name_collision_policy")),
				TEXT("fail_if_exists"),
				TEXT("reuse_if_exists"),
				OutError))
		{
			return false;
		}

		if (!TryCopyOptionalString(OpObject, TEXT("parent_component"), BuildOpFieldPath(TEXT("parent_component")), Payload, OutError) ||
			!TryCopyOptionalString(OpObject, TEXT("socket_name"), BuildOpFieldPath(TEXT("socket_name")), Payload, OutError) ||
			!TryCopyOptionalString(OpObject, TEXT("attach_rule"), BuildOpFieldPath(TEXT("attach_rule")), Payload, OutError) ||
			!TryCopyOptionalString(OpObject, TEXT("name_collision_policy"), BuildOpFieldPath(TEXT("name_collision_policy")), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	bool TryBuildSetComponentPropertiesPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		if (!TryValidateSettingsArray(OpObject, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyStringField(OpObject, TEXT("component_name"), BuildOpFieldPath(TEXT("component_name")), Payload, OutError))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Settings = nullptr;
		OpObject->TryGetArrayField(TEXT("settings"), Settings);
		if (Settings)
		{
			Payload->SetArrayField(TEXT("settings"), *Settings);
		}

		OutPayload = Payload;
		return true;
	}

	bool TryBuildRemoveComponentPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyStringField(OpObject, TEXT("component_name"), BuildOpFieldPath(TEXT("component_name")), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}
}

bool FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperComponentTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperComponentTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	FString AssetPath;
	TSharedPtr<FJsonObject> OpObject;
	if (!TryReadComponentTaskPlanParts(StepObject, AssetPath, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!TryReadRequiredString(OpObject, TEXT("op"), BuildOpFieldPath(TEXT("op")), OpName, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	if (OpName == OpAddComponent)
	{
		AdapterOperation = AdapterOperationAddComponent;
		if (!TryBuildAddComponentPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpSetComponentProperties)
	{
		AdapterOperation = AdapterOperationSetComponentProperties;
		if (!TryBuildSetComponentPropertiesPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpRemoveComponent)
	{
		AdapterOperation = AdapterOperationRemoveComponent;
		if (!TryBuildRemoveComponentPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		OutError = MakeComponentTaskPlanError(
			TEXT("unsupported_blueprint_component_op"),
			TEXT("Blueprint component adapter currently supports add_component, set_component_properties, and remove_component only."),
			BuildOpFieldPath(TEXT("op")));
		return false;
	}

	OutPayload.Capability = CapabilityBlueprintComponent;
	OutPayload.RuntimeOperation = RuntimeOperationBlueprintComponent;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}
