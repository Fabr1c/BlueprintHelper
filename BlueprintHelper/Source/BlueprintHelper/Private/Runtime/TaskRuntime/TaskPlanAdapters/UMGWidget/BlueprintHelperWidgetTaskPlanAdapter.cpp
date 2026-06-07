// BlueprintHelper TaskPlan adapter - UMG Widget Blueprint cluster.

#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperVersionCompat.h"

class FBlueprintHelperWidgetTaskPlanAdapterLocalUtils
{
public:
	static FBlueprintHelperToolError MakeWidgetTaskPlanError(
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

	static FString WidgetBuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	static FString WidgetBuildOpFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0].write.ops[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), *Suffix);
	}

	static FString WidgetJsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	static bool WidgetTryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Object, FieldName);
		if (!FoundValue.IsValid())
		{
			OutError = MakeWidgetTaskPlanError(ErrorCode, ErrorMessage, FieldPath);
			return false;
		}
		if (FoundValue->Type != EJson::String)
		{
			OutError = MakeWidgetTaskPlanError(
				ErrorCode,
				FString::Printf(
					TEXT("UMG widget field %s must be a string; actual type is %s."),
					FieldName,
					*WidgetJsonValueTypeToString(FoundValue)),
				FieldPath);
			return false;
		}

		OutValue = FoundValue->AsString();
		if (OutValue.IsEmpty())
		{
			OutError = MakeWidgetTaskPlanError(
				ErrorCode,
				FString::Printf(TEXT("UMG widget field %s cannot be empty."), FieldName),
				FieldPath);
			return false;
		}
		return true;
	}

	static bool WidgetTryCopyRequiredString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!WidgetTryReadRequiredString(
			Source,
			SourceFieldName,
			SourceFieldPath,
			TEXT("invalid_umg_widget_op"),
			FString::Printf(TEXT("UMG widget op requires %s."), SourceFieldName),
			Value,
			OutError))
		{
			return false;
		}

		Destination->SetStringField(DestinationFieldName, Value);
		return true;
	}

	static bool WidgetTryCopyOptionalString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Source, SourceFieldName);
		if (!FoundValue.IsValid())
		{
			return true;
		}
		if (FoundValue->Type != EJson::String)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				FString::Printf(
					TEXT("UMG widget op %s must be a string when present; actual type is %s."),
					SourceFieldName,
					*WidgetJsonValueTypeToString(FoundValue)),
				SourceFieldPath);
			return false;
		}

		Destination->SetStringField(DestinationFieldName, FoundValue->AsString());
		return true;
	}

	static bool WidgetTryCopyOptionalNumber(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Source, SourceFieldName);
		if (!FoundValue.IsValid())
		{
			return true;
		}
		if (FoundValue->Type != EJson::Number)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				FString::Printf(
					TEXT("UMG widget op %s must be a number when present; actual type is %s."),
					SourceFieldName,
					*WidgetJsonValueTypeToString(FoundValue)),
				SourceFieldPath);
			return false;
		}

		const double Number = FoundValue->AsNumber();
		if (Number < 0.0)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_virtual_index"),
				FString::Printf(TEXT("UMG widget op %s must be non-negative."), SourceFieldName),
				SourceFieldPath);
			return false;
		}

		Destination->SetNumberField(DestinationFieldName, Number);
		return true;
	}

	static bool WidgetTryCopyOptionalBool(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Source, SourceFieldName);
		if (!FoundValue.IsValid())
		{
			return true;
		}
		if (FoundValue->Type != EJson::Boolean)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				FString::Printf(
					TEXT("UMG widget op %s must be a bool when present; actual type is %s."),
					SourceFieldName,
					*WidgetJsonValueTypeToString(FoundValue)),
				SourceFieldPath);
			return false;
		}

		Destination->SetBoolField(DestinationFieldName, FoundValue->AsBool());
		return true;
	}

	static bool WidgetRejectLegacyPositionFields(
		const TSharedPtr<FJsonObject>& OpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TCHAR* LegacyFields[] = {
			TEXT("parent_widget_name"),
			TEXT("insert_index"),
			TEXT("child_index")
		};

		for (const TCHAR* FieldName : LegacyFields)
		{
			if (FBlueprintHelperVersionCompat::FindJsonValue(OpObject, FieldName).IsValid())
			{
				OutError = MakeWidgetTaskPlanError(
					TEXT("unsupported_umg_widget_legacy_position_field"),
					TEXT("UMG WidgetTree TaskPlan uses parent_name and virtual_index only."),
					WidgetBuildOpFieldPath(FieldName));
				return false;
			}
		}
		return true;
	}

	static bool WidgetTryReadPropertyName(
		const TSharedPtr<FJsonObject>& OpObject,
		FString& OutPropertyName,
		FBlueprintHelperToolError& OutError)
	{
		if (FBlueprintHelperVersionCompat::FindJsonValue(OpObject, TEXT("property_name")).IsValid())
		{
			return WidgetTryReadRequiredString(
				OpObject,
				TEXT("property_name"),
				WidgetBuildOpFieldPath(TEXT("property_name")),
				TEXT("invalid_umg_widget_op"),
				TEXT("set_widget_property requires property_name or property_path."),
				OutPropertyName,
				OutError);
		}

		return WidgetTryReadRequiredString(
			OpObject,
			TEXT("property_path"),
			WidgetBuildOpFieldPath(TEXT("property_path")),
			TEXT("invalid_umg_widget_op"),
			TEXT("set_widget_property requires property_name or property_path."),
			OutPropertyName,
			OutError);
	}

	static bool WidgetTryJsonValueToServiceImportText(
		const TSharedPtr<FJsonValue>& Value,
		const FString& FieldPath,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		if (!Value.IsValid() || Value->Type == EJson::None || Value->Type == EJson::Null)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				TEXT("set_widget_property requires a non-null value."),
				FieldPath);
			return false;
		}

		if (Value->Type == EJson::String)
		{
			OutValue = Value->AsString();
			return true;
		}
		if (Value->Type == EJson::Number)
		{
			OutValue = FString::SanitizeFloat(Value->AsNumber());
			return true;
		}
		if (Value->Type == EJson::Boolean)
		{
			OutValue = Value->AsBool() ? TEXT("true") : TEXT("false");
			return true;
		}

		FString SerializedValue;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedValue);
		if (!FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer))
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				TEXT("set_widget_property value could not be serialized for the current widget service."),
				FieldPath);
			return false;
		}

		OutValue = MoveTemp(SerializedValue);
		return true;
	}

	static bool WidgetTryReadTaskPlanParts(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutStepId,
		FString& OutAssetPath,
		FString& OutStrategy,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		OutStepId.Empty();
		OutAssetPath.Empty();
		OutStrategy.Empty();
		OutOpObject.Reset();

		if (!StepObject.IsValid())
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_step"),
				TEXT("UMG widget TaskPlan step must be an object."),
				WidgetBuildStepFieldPath(TEXT("")));
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
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_operation_field"),
				TEXT("UMG widget IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				WidgetBuildStepFieldPath(TEXT("operation")));
			return false;
		}

		FString Capability;
		if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
			Capability != FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_capability"),
				TEXT("UMG widget adapter only supports capability umg_widget."),
				WidgetBuildStepFieldPath(TEXT("capability")));
			return false;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_target"),
				TEXT("UMG widget TaskPlan step target object is required."),
				WidgetBuildStepFieldPath(TEXT("target")));
			return false;
		}

		if (!WidgetTryReadRequiredString(
			*TargetObjectPtr,
			TEXT("asset_path"),
			WidgetBuildStepFieldPath(TEXT("target.asset_path")),
			TEXT("invalid_umg_widget_target"),
			TEXT("UMG widget TaskPlan target requires asset_path."),
			OutAssetPath,
			OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_write"),
				TEXT("umg_widget TaskPlan step requires write object."),
				WidgetBuildStepFieldPath(TEXT("write")));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), OutStrategy) ||
			(OutStrategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit &&
			 OutStrategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit))
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy"),
				TEXT("UMG widget adapter currently supports widget_tree_edit and widget_property_edit strategies."),
				WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray || OpsArray->Num() != 1)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_ops"),
				TEXT("UMG widget adapter currently lowers exactly one write.ops entry per TaskPlan step."),
				WidgetBuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid()
			? (*OpsArray)[0]->AsObject()
			: nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				TEXT("UMG widget write.ops entry must be an object."),
				WidgetBuildOpFieldPath(TEXT("")));
			return false;
		}

		return true;
	}

	static bool WidgetTryBuildAddPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_class"), WidgetBuildOpFieldPath(TEXT("widget_class")), TEXT("widget_class"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		if (!WidgetTryCopyOptionalString(OpObject, TEXT("parent_name"), WidgetBuildOpFieldPath(TEXT("parent_name")), TEXT("parent_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("slot_name"), WidgetBuildOpFieldPath(TEXT("slot_name")), TEXT("slot_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("expected_parent_name"), WidgetBuildOpFieldPath(TEXT("expected_parent_name")), TEXT("expected_parent_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalNumber(OpObject, TEXT("virtual_index"), WidgetBuildOpFieldPath(TEXT("virtual_index")), TEXT("virtual_index"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildMovePayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("new_parent_name"), WidgetBuildOpFieldPath(TEXT("new_parent_name")), TEXT("new_parent_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("slot_name"), WidgetBuildOpFieldPath(TEXT("slot_name")), TEXT("slot_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("expected_parent_name"), WidgetBuildOpFieldPath(TEXT("expected_parent_name")), TEXT("expected_parent_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalNumber(OpObject, TEXT("virtual_index"), WidgetBuildOpFieldPath(TEXT("virtual_index")), TEXT("virtual_index"), Payload, OutError) ||
			!WidgetTryCopyOptionalNumber(OpObject, TEXT("expected_virtual_index"), WidgetBuildOpFieldPath(TEXT("expected_virtual_index")), TEXT("expected_virtual_index"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildSetNamedSlotContentPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("host_widget_name"), WidgetBuildOpFieldPath(TEXT("host_widget_name")), TEXT("host_widget_name"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("slot_name"), WidgetBuildOpFieldPath(TEXT("slot_name")), TEXT("slot_name"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("widget_class"), WidgetBuildOpFieldPath(TEXT("widget_class")), TEXT("widget_class"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalNumber(OpObject, TEXT("virtual_index"), WidgetBuildOpFieldPath(TEXT("virtual_index")), TEXT("virtual_index"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("expected_content_widget_name"), WidgetBuildOpFieldPath(TEXT("expected_content_widget_name")), TEXT("expected_content_widget_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalBool(OpObject, TEXT("replace_existing"), WidgetBuildOpFieldPath(TEXT("replace_existing")), TEXT("replace_existing"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildSetPropertyPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		FString PropertyName;
		if (!WidgetTryReadPropertyName(OpObject, PropertyName, OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("property_name"), PropertyName);

		const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(OpObject, TEXT("value"));
		FString ImportText;
		if (!WidgetTryJsonValueToServiceImportText(
			Value,
			WidgetBuildOpFieldPath(TEXT("value")),
			ImportText,
			OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("value"), ImportText);

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildSetSlotPropertyPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError) ||
			!WidgetTryCopyRequiredString(OpObject, TEXT("property_path"), WidgetBuildOpFieldPath(TEXT("property_path")), TEXT("property_path"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("expected_slot_class_path"), WidgetBuildOpFieldPath(TEXT("expected_slot_class_path")), TEXT("expected_slot_class_path"), Payload, OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(OpObject, TEXT("value"));
		FString ImportText;
		if (!WidgetTryJsonValueToServiceImportText(
			Value,
			WidgetBuildOpFieldPath(TEXT("value")),
			ImportText,
			OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("value"), ImportText);

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildSetWidgetAsVariablePayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError) ||
			!WidgetTryCopyOptionalString(OpObject, TEXT("expected_widget_class_path"), WidgetBuildOpFieldPath(TEXT("expected_widget_class_path")), TEXT("expected_widget_class_path"), Payload, OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonValue> IsVariableValue = FBlueprintHelperVersionCompat::FindJsonValue(OpObject, TEXT("is_variable"));
		if (!IsVariableValue.IsValid() || IsVariableValue->Type != EJson::Boolean)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				TEXT("set_widget_as_variable requires boolean is_variable."),
				WidgetBuildOpFieldPath(TEXT("is_variable")));
			return false;
		}
		Payload->SetBoolField(TEXT("is_variable"), IsVariableValue->AsBool());

		OutPayload = Payload;
		return true;
	}

	static bool WidgetTryBuildRemovePayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!WidgetTryCopyRequiredString(OpObject, TEXT("widget_name"), WidgetBuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

};

bool FBlueprintHelperWidgetTaskPlanAdapter::SupportsStep(const TSharedPtr<FJsonObject>& StepObject)
{
	FString Capability;
	return StepObject.IsValid() &&
		StepObject->TryGetStringField(TEXT("capability"), Capability) &&
		Capability == FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
}

bool FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperWidgetTaskPlanLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	OutLoweredStep = FBlueprintHelperWidgetTaskPlanLoweredStep();

	FBlueprintHelperWidgetTaskPlanPayload Payload;
	if (!TryBuildPayloadFromTaskPlanStep(TaskPlan, StepObject, bDryRun, Payload, OutError))
	{
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutLoweredStep.StepId);
	if (OutLoweredStep.StepId.IsEmpty())
	{
		OutLoweredStep.StepId = TEXT("step_001");
	}

	FBlueprintHelperWidgetTaskPlanLoweredOp LoweredOp;
	LoweredOp.AdapterOperation = Payload.AdapterOperation;
	LoweredOp.Payload = Payload.Payload;

	OutLoweredStep.Capability = Payload.Capability;
	OutLoweredStep.RuntimeOperation = Payload.RuntimeOperation;
	OutLoweredStep.Ops.Add(MoveTemp(LoweredOp));
	OutLoweredStep.bAdapterDryRunSupported = Payload.bAdapterDryRunSupported;
	return true;
}

bool FBlueprintHelperWidgetTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& /*TaskPlan*/,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperWidgetTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperWidgetTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	FString StepId;
	FString AssetPath;
	FString Strategy;
	TSharedPtr<FJsonObject> OpObject;
	if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryReadTaskPlanParts(StepObject, StepId, AssetPath, Strategy, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryReadRequiredString(
		OpObject,
		TEXT("op"),
		FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildOpFieldPath(TEXT("op")),
		TEXT("invalid_umg_widget_op"),
		TEXT("UMG widget op requires op."),
		OpName,
		OutError))
	{
		return false;
	}
	if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetRejectLegacyPositionFields(
		OpObject,
		OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	if (OpName == FBlueprintHelperWidgetTaskPlan::Op::AddWidget)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("add_widget requires widget_tree_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildAddPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::MoveWidget)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("move_widget requires widget_tree_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::MoveWidget;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildMovePayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::SetNamedSlotContent)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("set_named_slot_content requires widget_tree_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildSetNamedSlotContentPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::SetWidgetProperty)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("set_widget_property requires widget_property_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildSetPropertyPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::SetSlotProperty)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("set_slot_property requires widget_property_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildSetSlotPropertyPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::SetWidgetAsVariable)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("set_widget_as_variable requires widget_tree_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildSetWidgetAsVariablePayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == FBlueprintHelperWidgetTaskPlan::Op::RemoveWidget)
	{
		if (Strategy != FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("remove_widget requires widget_tree_edit strategy."),
				FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget;
		if (!FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetTryBuildRemovePayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		OutError = FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::MakeWidgetTaskPlanError(
			TEXT("unsupported_umg_widget_op"),
			TEXT("UMG widget adapter supports add_widget, move_widget, set_named_slot_content, set_widget_property, set_slot_property, set_widget_as_variable, and remove_widget."),
			FBlueprintHelperWidgetTaskPlanAdapterLocalUtils::WidgetBuildOpFieldPath(TEXT("op")));
		return false;
	}

	OutPayload.Capability = FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
	OutPayload.RuntimeOperation = FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}
