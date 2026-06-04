// BlueprintHelper TaskPlan adapter - Blueprint Component cluster.

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent = TEXT("blueprint_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree = TEXT("component_tree");

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::RuntimeOperationBlueprintComponent = TEXT("blueprint_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent = TEXT("add_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties = TEXT("set_component_properties");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRenameComponent = TEXT("rename_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationReparentComponent = TEXT("reparent_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAttachComponent = TEXT("attach_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationDetachComponent = TEXT("detach_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetRootComponent = TEXT("set_root_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent = TEXT("remove_component");

const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent = TEXT("add_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpSetComponentProperties = TEXT("set_component_properties");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpRenameComponent = TEXT("rename_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpReparentComponent = TEXT("reparent_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpAttachComponent = TEXT("attach_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpDetachComponent = TEXT("detach_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpSetRootComponent = TEXT("set_root_component");
const TCHAR* FBlueprintHelperComponentTaskPlanAdapter::OpRemoveComponent = TEXT("remove_component");

class FBlueprintHelperComponentTaskPlanAdapterLocalUtils
{
public:
	static FString ComponentJsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	static FBlueprintHelperToolError MakeComponentTaskPlanError(
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

	static FString ComponentBuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	static FString ComponentBuildOpFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0].write.ops[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), *Suffix);
	}

	static bool ComponentTryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Object, FieldName);
		if (!FoundValue.IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(TEXT("Blueprint component op requires %s."), FieldName),
				FieldPath);
			return false;
		}
		if (FoundValue->Type != EJson::String)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(TEXT("Blueprint component op %s must be a string."), FieldName),
				FieldPath);
			return false;
		}

		OutValue = FoundValue->AsString();
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

	static bool ComponentTryCopyOptionalString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Source, FieldName);
		if (!FoundValue.IsValid())
		{
			return true;
		}
		if (FoundValue->Type != EJson::String)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_op"),
				FString::Printf(
					TEXT("Blueprint component op %s must be a string when present; actual type is %s."),
					FieldName,
					*ComponentJsonValueTypeToString(FoundValue)),
				FieldPath);
			return false;
		}

		Destination->SetStringField(FieldName, FoundValue->AsString());
		return true;
	}

	static bool ComponentTryValidateStringEnum(
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

	static TArray<FString> ComponentAllowedValues(
		const TCHAR* A,
		const TCHAR* B,
		const TCHAR* C = nullptr,
		const TCHAR* D = nullptr)
	{
		TArray<FString> Values;
		if (A) Values.Add(A);
		if (B) Values.Add(B);
		if (C) Values.Add(C);
		if (D) Values.Add(D);
		return Values;
	}

	static bool ComponentTryValidateStringEnumValues(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TArray<FString>& AllowedValues,
		const FString& ErrorCode,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!Source.IsValid() || !Source->TryGetStringField(FieldName, Value))
		{
			return true;
		}
		if (!AllowedValues.Contains(Value))
		{
			OutError = MakeComponentTaskPlanError(
				ErrorCode,
				FString::Printf(TEXT("Unsupported blueprint component %s value: %s."), FieldName, *Value),
				FieldPath);
			return false;
		}
		return true;
	}

	static bool ComponentTryCopyStringField(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!ComponentTryReadRequiredString(Source, FieldName, FieldPath, Value, OutError))
		{
			return false;
		}
		Destination->SetStringField(FieldName, Value);
		return true;
	}

	static bool ComponentTryReadTaskPlanParts(
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
				ComponentBuildStepFieldPath(TEXT("")));
			return false;
		}

		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_operation_field"),
				TEXT("Blueprint component TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				ComponentBuildStepFieldPath(TEXT("operation")));
			return false;
		}

		FString Capability;
		if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
			Capability != FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_capability"),
				TEXT("Blueprint component adapter only supports capability blueprint_component."),
				ComponentBuildStepFieldPath(TEXT("capability")));
			return false;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_target"),
				TEXT("Blueprint component TaskPlan step target object is required."),
				ComponentBuildStepFieldPath(TEXT("target")));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) ||
			OutAssetPath.IsEmpty())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_target"),
				TEXT("Blueprint component TaskPlan step target requires asset_path."),
				ComponentBuildStepFieldPath(TEXT("target.asset_path")));
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_write"),
				TEXT("blueprint_component TaskPlan step requires write object."),
				ComponentBuildStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("unsupported_blueprint_component_strategy"),
				TEXT("Blueprint component adapter currently supports component_tree strategy only."),
				ComponentBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray || OpsArray->Num() != 1)
		{
			OutError = MakeComponentTaskPlanError(
				TEXT("invalid_blueprint_component_ops"),
				TEXT("Blueprint component adapter currently lowers exactly one write.ops entry per TaskPlan step."),
				ComponentBuildStepFieldPath(TEXT("write.ops")));
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
				ComponentBuildOpFieldPath(TEXT("")));
			return false;
		}

		return true;
	}

	static bool ComponentTryValidateSettingsArray(
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
				ComponentBuildOpFieldPath(TEXT("settings")));
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
			if (!ComponentTryReadRequiredString(
				SettingObject,
				TEXT("property_path"),
				SettingPath + TEXT(".property_path"),
				PropertyPath,
				OutError))
			{
				OutError.Code = TEXT("invalid_blueprint_component_property_setting");
				return false;
			}

			if (!FBlueprintHelperVersionCompat::FindJsonValue(SettingObject, TEXT("value")).IsValid())
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

	static bool ComponentTryBuildAddPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!ComponentTryCopyStringField(OpObject, TEXT("component_class"), ComponentBuildOpFieldPath(TEXT("component_class")), Payload, OutError))
		{
			return false;
		}

		FString NameCollisionPolicy;
		if (OpObject.IsValid() &&
			OpObject->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy) &&
			NameCollisionPolicy.Equals(TEXT("reuse_existing"), ESearchCase::IgnoreCase))
		{
			OpObject->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));
		}

		if (!ComponentTryValidateStringEnumValues(
				OpObject,
				TEXT("attach_rule"),
				ComponentBuildOpFieldPath(TEXT("attach_rule")),
				ComponentAllowedValues(
					TEXT("keep_relative"),
					TEXT("keep_world"),
					TEXT("snap_to_target")),
				TEXT("unsupported_attach_rule"),
				OutError) ||
			!ComponentTryValidateStringEnumValues(
				OpObject,
				TEXT("name_collision_policy"),
				ComponentBuildOpFieldPath(TEXT("name_collision_policy")),
				ComponentAllowedValues(
					TEXT("fail_if_exists"),
					TEXT("reuse_if_exists"),
					TEXT("block_if_class_mismatch")),
				TEXT("unsupported_name_collision_policy"),
				OutError))
		{
			return false;
		}

		const FBlueprintHelperComponentToolClusterPolicy Policy =
			FBlueprintHelperToolClusterConfigResolver::LoadComponentPolicy();
		if (OpObject.IsValid() && !OpObject->HasField(TEXT("attach_rule")) && !Policy.DefaultAttachRule.IsEmpty())
		{
			Payload->SetStringField(TEXT("attach_rule"), Policy.DefaultAttachRule);
		}
		if (OpObject.IsValid() && !OpObject->HasField(TEXT("name_collision_policy")) && !Policy.DefaultNameCollisionPolicy.IsEmpty())
		{
			Payload->SetStringField(TEXT("name_collision_policy"), Policy.DefaultNameCollisionPolicy);
		}
		if (OpObject.IsValid() && !OpObject->HasField(TEXT("property_mode")) && !Policy.DefaultPropertyMode.IsEmpty())
		{
			Payload->SetStringField(TEXT("property_mode"), Policy.DefaultPropertyMode);
		}

		if (!ComponentTryCopyOptionalString(OpObject, TEXT("parent_component"), ComponentBuildOpFieldPath(TEXT("parent_component")), Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("socket_name"), ComponentBuildOpFieldPath(TEXT("socket_name")), Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("attach_rule"), ComponentBuildOpFieldPath(TEXT("attach_rule")), Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("name_collision_policy"), ComponentBuildOpFieldPath(TEXT("name_collision_policy")), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildSetPropertiesPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		if (!ComponentTryValidateSettingsArray(OpObject, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError))
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

	static bool ComponentTryCopyTransformPolicy(
		const TSharedPtr<FJsonObject>& OpObject,
		const TSharedRef<FJsonObject>& Payload,
		FBlueprintHelperToolError& OutError)
	{
		if (!ComponentTryValidateStringEnumValues(
			OpObject,
			TEXT("transform_policy"),
			ComponentBuildOpFieldPath(TEXT("transform_policy")),
			ComponentAllowedValues(TEXT("preserve_world"), TEXT("preserve_relative"), TEXT("reset_relative")),
			TEXT("unsupported_transform_policy"),
			OutError))
		{
			return false;
		}
		return ComponentTryCopyOptionalString(
			OpObject,
			TEXT("transform_policy"),
			ComponentBuildOpFieldPath(TEXT("transform_policy")),
			Payload,
			OutError);
	}

	static bool ComponentTryCopyDefaultRootPolicy(
		const TSharedPtr<FJsonObject>& OpObject,
		const TSharedRef<FJsonObject>& Payload,
		FBlueprintHelperToolError& OutError)
	{
		if (!ComponentTryValidateStringEnumValues(
			OpObject,
			TEXT("default_root_policy"),
			ComponentBuildOpFieldPath(TEXT("default_root_policy")),
			ComponentAllowedValues(TEXT("require_scene_component"), TEXT("create_default_scene_root_when_needed")),
			TEXT("unsupported_default_root_policy"),
			OutError))
		{
			return false;
		}
		return ComponentTryCopyOptionalString(
			OpObject,
			TEXT("default_root_policy"),
			ComponentBuildOpFieldPath(TEXT("default_root_policy")),
			Payload,
			OutError);
	}

	static bool ComponentTryBuildRenamePayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!ComponentTryCopyStringField(OpObject, TEXT("new_component_name"), ComponentBuildOpFieldPath(TEXT("new_component_name")), Payload, OutError))
		{
			return false;
		}
		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildReparentPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!ComponentTryCopyStringField(OpObject, TEXT("new_parent_component"), ComponentBuildOpFieldPath(TEXT("new_parent_component")), Payload, OutError))
		{
			return false;
		}
		if (!ComponentTryValidateStringEnumValues(
				OpObject,
				TEXT("attach_rule"),
				ComponentBuildOpFieldPath(TEXT("attach_rule")),
				ComponentAllowedValues(
					TEXT("keep_relative"),
					TEXT("keep_world"),
					TEXT("snap_to_target")),
				TEXT("unsupported_attach_rule"),
				OutError) ||
			!ComponentTryCopyTransformPolicy(OpObject, Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("socket_name"), ComponentBuildOpFieldPath(TEXT("socket_name")), Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("attach_rule"), ComponentBuildOpFieldPath(TEXT("attach_rule")), Payload, OutError))
		{
			return false;
		}
		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildAttachPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!ComponentTryCopyStringField(OpObject, TEXT("parent_component"), ComponentBuildOpFieldPath(TEXT("parent_component")), Payload, OutError))
		{
			return false;
		}
		if (!ComponentTryValidateStringEnumValues(
				OpObject,
				TEXT("attach_rule"),
				ComponentBuildOpFieldPath(TEXT("attach_rule")),
				ComponentAllowedValues(
					TEXT("keep_relative"),
					TEXT("keep_world"),
					TEXT("snap_to_target")),
				TEXT("unsupported_attach_rule"),
				OutError) ||
			!ComponentTryCopyTransformPolicy(OpObject, Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("socket_name"), ComponentBuildOpFieldPath(TEXT("socket_name")), Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("attach_rule"), ComponentBuildOpFieldPath(TEXT("attach_rule")), Payload, OutError))
		{
			return false;
		}
		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildDetachPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError) ||
			!ComponentTryCopyTransformPolicy(OpObject, Payload, OutError) ||
			!ComponentTryCopyDefaultRootPolicy(OpObject, Payload, OutError))
		{
			return false;
		}
		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildSetRootPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError))
		{
			return false;
		}
		if (!ComponentTryValidateStringEnumValues(
				OpObject,
				TEXT("old_root_policy"),
				ComponentBuildOpFieldPath(TEXT("old_root_policy")),
				ComponentAllowedValues(TEXT("keep_as_child"), TEXT("remove_default_scene_root_when_empty")),
				TEXT("unsupported_old_root_policy"),
				OutError) ||
			!ComponentTryCopyDefaultRootPolicy(OpObject, Payload, OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("old_root_policy"), ComponentBuildOpFieldPath(TEXT("old_root_policy")), Payload, OutError))
		{
			return false;
		}
		OutPayload = Payload;
		return true;
	}

	static bool ComponentTryBuildRemovePayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!ComponentTryCopyStringField(OpObject, TEXT("component_name"), ComponentBuildOpFieldPath(TEXT("component_name")), Payload, OutError))
		{
			return false;
		}
		if (!ComponentTryValidateStringEnumValues(
			OpObject,
			TEXT("delete_policy"),
			ComponentBuildOpFieldPath(TEXT("delete_policy")),
			ComponentAllowedValues(TEXT("block_if_children"), TEXT("promote_children"), TEXT("delete_owned_children"), TEXT("reattach_children_to_parent")),
			TEXT("unsupported_delete_policy"),
			OutError) ||
			!ComponentTryCopyOptionalString(OpObject, TEXT("delete_policy"), ComponentBuildOpFieldPath(TEXT("delete_policy")), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	static bool ComponentResolveDryRun(const TSharedPtr<FJsonObject>& OpObject, bool bTaskPlanDryRun)
	{
		bool bDryRun = FBlueprintHelperToolClusterConfigResolver::LoadComponentPolicy().bDryRun;
		if (bTaskPlanDryRun)
		{
			bDryRun = true;
		}
		if (OpObject.IsValid() && OpObject->HasField(TEXT("dry_run")))
		{
			OpObject->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}
		return bDryRun;
	}

};

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
	if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryReadTaskPlanParts(StepObject, AssetPath, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryReadRequiredString(OpObject, TEXT("op"), FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentBuildOpFieldPath(TEXT("op")), OpName, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	const bool bEffectiveDryRun =
		FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentResolveDryRun(OpObject, bDryRun);
	if (OpName == OpAddComponent)
	{
		AdapterOperation = AdapterOperationAddComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildAddPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpSetComponentProperties)
	{
		AdapterOperation = AdapterOperationSetComponentProperties;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildSetPropertiesPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpRenameComponent)
	{
		AdapterOperation = AdapterOperationRenameComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildRenamePayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpReparentComponent)
	{
		AdapterOperation = AdapterOperationReparentComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildReparentPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpAttachComponent)
	{
		AdapterOperation = AdapterOperationAttachComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildAttachPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpDetachComponent)
	{
		AdapterOperation = AdapterOperationDetachComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildDetachPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpSetRootComponent)
	{
		AdapterOperation = AdapterOperationSetRootComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildSetRootPayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == OpRemoveComponent)
	{
		AdapterOperation = AdapterOperationRemoveComponent;
		if (!FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentTryBuildRemovePayload(AssetPath, OpObject, bEffectiveDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		OutError = FBlueprintHelperComponentTaskPlanAdapterLocalUtils::MakeComponentTaskPlanError(
			TEXT("unsupported_blueprint_component_op"),
			TEXT("Blueprint component adapter supports add_component, set_component_properties, rename_component, reparent_component, attach_component, detach_component, set_root_component, and remove_component."),
			FBlueprintHelperComponentTaskPlanAdapterLocalUtils::ComponentBuildOpFieldPath(TEXT("op")));
		return false;
	}

	OutPayload.Capability = CapabilityBlueprintComponent;
	OutPayload.RuntimeOperation = RuntimeOperationBlueprintComponent;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}
