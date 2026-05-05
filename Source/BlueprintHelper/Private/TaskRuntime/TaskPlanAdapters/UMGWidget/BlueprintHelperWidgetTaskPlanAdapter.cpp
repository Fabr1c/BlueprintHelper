// BlueprintHelper TaskPlan adapter - UMG Widget Blueprint cluster.

#include "TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FBlueprintHelperToolError MakeWidgetTaskPlanError(
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

	bool TryReadRequiredString(
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
			OutError = MakeWidgetTaskPlanError(ErrorCode, ErrorMessage, FieldPath);
			return false;
		}
		if ((*FoundValue)->Type != EJson::String)
		{
			OutError = MakeWidgetTaskPlanError(
				ErrorCode,
				FString::Printf(
					TEXT("UMG widget field %s must be a string; actual type is %s."),
					FieldName,
					*JsonValueTypeToString(*FoundValue)),
				FieldPath);
			return false;
		}

		OutValue = (*FoundValue)->AsString();
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

	bool TryCopyRequiredString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		FString Value;
		if (!TryReadRequiredString(
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

	bool TryCopyOptionalString(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceFieldName,
		const FString& SourceFieldPath,
		const TCHAR* DestinationFieldName,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonValue>* FoundValue = Source.IsValid()
			? Source->Values.Find(SourceFieldName)
			: nullptr;
		if (!FoundValue)
		{
			return true;
		}
		if (!FoundValue->IsValid() || (*FoundValue)->Type != EJson::String)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_op"),
				FString::Printf(
					TEXT("UMG widget op %s must be a string when present; actual type is %s."),
					SourceFieldName,
					*JsonValueTypeToString(FoundValue ? *FoundValue : nullptr)),
				SourceFieldPath);
			return false;
		}

		Destination->SetStringField(DestinationFieldName, (*FoundValue)->AsString());
		return true;
	}

	bool TryReadPropertyName(
		const TSharedPtr<FJsonObject>& OpObject,
		FString& OutPropertyName,
		FBlueprintHelperToolError& OutError)
	{
		if (OpObject.IsValid() && OpObject->Values.Contains(TEXT("property_name")))
		{
			return TryReadRequiredString(
				OpObject,
				TEXT("property_name"),
				BuildOpFieldPath(TEXT("property_name")),
				TEXT("invalid_umg_widget_op"),
				TEXT("set_widget_property requires property_name or property_path."),
				OutPropertyName,
				OutError);
		}

		return TryReadRequiredString(
			OpObject,
			TEXT("property_path"),
			BuildOpFieldPath(TEXT("property_path")),
			TEXT("invalid_umg_widget_op"),
			TEXT("set_widget_property requires property_name or property_path."),
			OutPropertyName,
			OutError);
	}

	bool TryJsonValueToServiceImportText(
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

	bool TryReadWidgetTaskPlanParts(
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
				BuildStepFieldPath(TEXT("")));
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
				BuildStepFieldPath(TEXT("operation")));
			return false;
		}

		FString Capability;
		if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
			Capability != BlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_capability"),
				TEXT("UMG widget adapter only supports capability umg_widget."),
				BuildStepFieldPath(TEXT("capability")));
			return false;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_target"),
				TEXT("UMG widget TaskPlan step target object is required."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		if (!TryReadRequiredString(
			*TargetObjectPtr,
			TEXT("asset_path"),
			BuildStepFieldPath(TEXT("target.asset_path")),
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
				BuildStepFieldPath(TEXT("write")));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), OutStrategy) ||
			(OutStrategy != BlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit &&
			 OutStrategy != BlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit))
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy"),
				TEXT("UMG widget adapter currently supports widget_tree_edit and widget_property_edit strategies."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) ||
			!OpsArray || OpsArray->Num() != 1)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("invalid_umg_widget_ops"),
				TEXT("UMG widget adapter currently lowers exactly one write.ops entry per TaskPlan step."),
				BuildStepFieldPath(TEXT("write.ops")));
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
				BuildOpFieldPath(TEXT("")));
			return false;
		}

		return true;
	}

	bool TryBuildAddWidgetPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyRequiredString(OpObject, TEXT("widget_class"), BuildOpFieldPath(TEXT("widget_class")), TEXT("widget_class"), Payload, OutError) ||
			!TryCopyRequiredString(OpObject, TEXT("widget_name"), BuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		if (OpObject.IsValid() && OpObject->Values.Contains(TEXT("parent_widget_name")))
		{
			if (!TryCopyOptionalString(OpObject, TEXT("parent_widget_name"), BuildOpFieldPath(TEXT("parent_widget_name")), TEXT("parent_name"), Payload, OutError))
			{
				return false;
			}
		}
		else if (!TryCopyOptionalString(OpObject, TEXT("parent_name"), BuildOpFieldPath(TEXT("parent_name")), TEXT("parent_name"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}

	bool TryBuildSetWidgetPropertyPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyRequiredString(OpObject, TEXT("widget_name"), BuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		FString PropertyName;
		if (!TryReadPropertyName(OpObject, PropertyName, OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("property_name"), PropertyName);

		const TSharedPtr<FJsonValue>* Value = OpObject.IsValid()
			? OpObject->Values.Find(TEXT("value"))
			: nullptr;
		FString ImportText;
		if (!TryJsonValueToServiceImportText(
			Value ? *Value : nullptr,
			BuildOpFieldPath(TEXT("value")),
			ImportText,
			OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("value"), ImportText);

		OutPayload = Payload;
		return true;
	}

	bool TryBuildRemoveWidgetPayload(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		if (!TryCopyRequiredString(OpObject, TEXT("widget_name"), BuildOpFieldPath(TEXT("widget_name")), TEXT("widget_name"), Payload, OutError))
		{
			return false;
		}

		OutPayload = Payload;
		return true;
	}
}

bool FBlueprintHelperWidgetTaskPlanAdapter::SupportsStep(const TSharedPtr<FJsonObject>& StepObject)
{
	FString Capability;
	return StepObject.IsValid() &&
		StepObject->TryGetStringField(TEXT("capability"), Capability) &&
		Capability == BlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
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
	if (!TryReadWidgetTaskPlanParts(StepObject, StepId, AssetPath, Strategy, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!TryReadRequiredString(
		OpObject,
		TEXT("op"),
		BuildOpFieldPath(TEXT("op")),
		TEXT("invalid_umg_widget_op"),
		TEXT("UMG widget op requires op."),
		OpName,
		OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	if (OpName == BlueprintHelperWidgetTaskPlan::Op::AddWidget)
	{
		if (Strategy != BlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("add_widget requires widget_tree_edit strategy."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = BlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget;
		if (!TryBuildAddWidgetPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == BlueprintHelperWidgetTaskPlan::Op::SetWidgetProperty)
	{
		if (Strategy != BlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("set_widget_property requires widget_property_edit strategy."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = BlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty;
		if (!TryBuildSetWidgetPropertyPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (OpName == BlueprintHelperWidgetTaskPlan::Op::RemoveWidget)
	{
		if (Strategy != BlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
		{
			OutError = MakeWidgetTaskPlanError(
				TEXT("unsupported_umg_widget_strategy_for_op"),
				TEXT("remove_widget requires widget_tree_edit strategy."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}
		AdapterOperation = BlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget;
		if (!TryBuildRemoveWidgetPayload(AssetPath, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		OutError = MakeWidgetTaskPlanError(
			TEXT("unsupported_umg_widget_op"),
			TEXT("UMG widget adapter currently supports add_widget, set_widget_property, and remove_widget only."),
			BuildOpFieldPath(TEXT("op")));
		return false;
	}

	OutPayload.Capability = BlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
	OutPayload.RuntimeOperation = BlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = false;
	return true;
}
