// BlueprintHelper TaskRuntime cluster execution utilities.

#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static FBlueprintHelperToolError MakeTaskRuntimeError(
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

static FString SerializeTaskRuntimeReviewPayload(const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return TEXT("");
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
	return Serialized;
}

static FString MakeTaskRuntimeReviewRefSegment(const FString& RawValue)
{
	FString Segment;
	Segment.Reserve(RawValue.Len());
	for (const TCHAR Ch : RawValue)
	{
		Segment.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
	}
	while (Segment.Contains(TEXT("__")))
	{
		Segment.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	return Segment.IsEmpty() ? FString(TEXT("target")) : Segment;
}

static EBlueprintHelperReviewChangeKind DeriveTaskRuntimeReviewChangeKind(const FString& OperationKind)
{
	if (OperationKind.Contains(TEXT("remove"), ESearchCase::IgnoreCase) ||
		OperationKind.Contains(TEXT("delete"), ESearchCase::IgnoreCase) ||
		OperationKind.Contains(TEXT("cleanup"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewChangeKind::Removed;
	}
	if (OperationKind.Contains(TEXT("add"), ESearchCase::IgnoreCase) ||
		OperationKind.Contains(TEXT("create"), ESearchCase::IgnoreCase) ||
		OperationKind.Contains(TEXT("ensure"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewChangeKind::Added;
	}
	return EBlueprintHelperReviewChangeKind::Modified;
}

static FString ReadTaskRuntimeReviewStringField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	FString Value;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(FieldName, Value);
	}
	return Value;
}

static TSharedPtr<FJsonObject> ReadTaskRuntimeReviewObjectField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
	if (Payload.IsValid() &&
		Payload->TryGetObjectField(FieldName, ObjectPtr) &&
		ObjectPtr &&
		ObjectPtr->IsValid())
	{
		return *ObjectPtr;
	}
	return nullptr;
}

static FString ReadTaskRuntimeReviewNestedStringField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* ObjectFieldName,
	const TCHAR* FieldName)
{
	return ReadTaskRuntimeReviewStringField(
		ReadTaskRuntimeReviewObjectField(Payload, ObjectFieldName),
		FieldName);
}

static FString ReadTaskRuntimeReviewGraphName(const TSharedPtr<FJsonObject>& Payload)
{
	FString GraphName = ReadTaskRuntimeReviewStringField(Payload, TEXT("graph"));
	if (GraphName.IsEmpty())
	{
		GraphName = ReadTaskRuntimeReviewStringField(Payload, TEXT("graph_name"));
	}
	if (GraphName.IsEmpty())
	{
		GraphName = ReadTaskRuntimeReviewNestedStringField(Payload, TEXT("target"), TEXT("graph"));
	}
	if (GraphName.IsEmpty())
	{
		GraphName = ReadTaskRuntimeReviewNestedStringField(Payload, TEXT("target"), TEXT("graph_name"));
	}
	return GraphName;
}

static FString NormalizeTaskRuntimeSignatureKind(FString Value)
{
	Value.TrimStartAndEndInline();
	Value.ToLowerInline();
	return Value;
}

static FString ResolveTaskRuntimeSignatureReviewSubKind(
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	if (AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureFunction)
	{
		return TEXT("function");
	}
	if (AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureCustomEvent)
	{
		return TEXT("custom_event");
	}
	if (AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureMacro)
	{
		return TEXT("macro");
	}
	if (AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureEventDispatcher)
	{
		return TEXT("dispatcher");
	}
	if (AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureOverrideEvent)
	{
		return TEXT("override_event");
	}
	if (AdapterOperation != FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationRemoveSignature)
	{
		return TEXT("");
	}

	const FString SignatureKind = NormalizeTaskRuntimeSignatureKind(
		ReadTaskRuntimeReviewStringField(Payload, TEXT("signature_kind")));
	if (SignatureKind == TEXT("function") || SignatureKind == TEXT("interface_function"))
	{
		return TEXT("function");
	}
	if (SignatureKind == TEXT("custom_event") || SignatureKind == TEXT("interface_event"))
	{
		return TEXT("custom_event");
	}
	if (SignatureKind == TEXT("macro"))
	{
		return TEXT("macro");
	}
	if (SignatureKind == TEXT("event_dispatcher") || SignatureKind == TEXT("dispatcher"))
	{
		return TEXT("dispatcher");
	}
	if (SignatureKind == TEXT("override_event") || SignatureKind == TEXT("native_event"))
	{
		return TEXT("override_event");
	}
	return TEXT("");
}

static FBlueprintHelperReviewAtomicTarget* AddTaskRuntimeReviewTarget(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& TargetName,
	const FString& VisualGroupPrefix,
	const FString& DisplayLabel,
	const FString& GraphName = TEXT(""))
{
	if (TargetName.IsEmpty())
	{
		return nullptr;
	}

	const FString SafeTargetName = MakeTaskRuntimeReviewRefSegment(TargetName);
	const FString TargetKey = FString::Printf(TEXT("%s:%s"), *TargetKind, *SafeTargetName);
	const FString VisualGroupKey = FString::Printf(TEXT("%s:%s"), *VisualGroupPrefix, *SafeTargetName);
	const FString PayloadText = SerializeTaskRuntimeReviewPayload(Payload);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Evidence.AssetPath;
	Target.GraphName = GraphName;
	Target.TargetKey = TargetKey;
	Target.TargetKind = TargetKind;
	if (FBlueprintHelperReviewTargetKindRegistry::IsComponentTargetKind(TargetKind))
	{
		Target.ComponentPath = TargetName;
	}
	if (FBlueprintHelperReviewTargetKindRegistry::IsPropertyTargetKind(TargetKind))
	{
		Target.PropertyPath = TargetName;
	}
	Target.VisualGroupKey = VisualGroupKey;
	Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
		Surface,
		Target.TargetKind,
		Target.TargetKey,
		Target.VisualGroupKey,
		Evidence.OperationKind);
	Target.DisplayLabel = DisplayLabel.IsEmpty() ? TargetName : DisplayLabel;
	Target.LatestEvidenceId = Evidence.EvidenceId;
	Target.SourceEvidenceIds.Add(Evidence.EvidenceId);
	Target.AnchorJson = PayloadText;
	Target.Ownership = TEXT("blueprinthelper_owned");
	const int32 TargetIndex = Evidence.AtomicTargets.Add(Target);
	return Evidence.AtomicTargets.IsValidIndex(TargetIndex)
		? &Evidence.AtomicTargets[TargetIndex]
		: nullptr;
}

static void AddTaskRuntimeReviewTargetsFromStringArray(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& VisualGroupPrefix,
	const FString& DisplayPrefix)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString TargetName;
		if (Value.IsValid() && Value->TryGetString(TargetName) && !TargetName.IsEmpty())
		{
			AddTaskRuntimeReviewTarget(
				Evidence,
				Payload,
				Surface,
				TargetKind,
				TargetName,
				VisualGroupPrefix,
				DisplayPrefix.IsEmpty() ? TargetName : DisplayPrefix + TEXT(" ") + TargetName);
		}
	}
}

static void AddTaskRuntimeReviewTargetsFromObjectArray(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName,
	const TCHAR* NameFieldName,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& VisualGroupPrefix,
	const FString& DisplayPrefix)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString TargetName;
		if (Object.IsValid() && Object->TryGetStringField(NameFieldName, TargetName) && !TargetName.IsEmpty())
		{
			AddTaskRuntimeReviewTarget(
				Evidence,
				Payload,
				Surface,
				TargetKind,
				TargetName,
				VisualGroupPrefix,
				DisplayPrefix.IsEmpty() ? TargetName : DisplayPrefix + TEXT(" ") + TargetName);
		}
	}
}

static void AddTaskRuntimeClassDefaultReviewTargets(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString TargetName;
		if (!Object.IsValid() || !Object->TryGetStringField(TEXT("property_path"), TargetName) || TargetName.IsEmpty())
		{
			continue;
		}

		FString MutationStrategy;
		Object->TryGetStringField(TEXT("mutation_strategy"), MutationStrategy);
		const bool bSetterAware = MutationStrategy.Equals(TEXT("setter_aware_property"), ESearchCase::IgnoreCase);
		FBlueprintHelperReviewAtomicTarget* Target = AddTaskRuntimeReviewTarget(
			Evidence,
			Payload,
			EBlueprintHelperReviewSurface::Details,
			bSetterAware ? TEXT("class_default_setter_property") : TEXT("class_default_property"),
			TargetName,
			TEXT("class_setting"),
			FString::Printf(TEXT("class default %s"), *TargetName));
		if (Target && bSetterAware)
		{
			Target->TargetSubKind = TEXT("setter_aware_property");
		}
	}
}

static TSharedRef<FJsonObject> MakeRuntimeTarget(
	const FString& AssetPath,
	const FString& TargetType,
	const FString& MemberName = TEXT(""),
	const FString& RowName = TEXT(""),
	const FString& PropertyPath = TEXT(""))
{
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	if (!AssetPath.IsEmpty())
	{
		Target->SetStringField(TEXT("asset_path"), AssetPath);
	}
	Target->SetStringField(TEXT("target_type"), TargetType);
	if (!MemberName.IsEmpty())
	{
		Target->SetStringField(TEXT("member_name"), MemberName);
	}
	if (!RowName.IsEmpty())
	{
		Target->SetStringField(TEXT("row_name"), RowName);
	}
	if (!PropertyPath.IsEmpty())
	{
		Target->SetStringField(TEXT("property_path"), PropertyPath);
	}
	return Target;
}

static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return TEXT("");
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		{
			const double NumberValue = Value->AsNumber();
			const double RoundedValue = FMath::RoundToDouble(NumberValue);
			if (FMath::IsNearlyEqual(NumberValue, RoundedValue))
			{
				return FString::Printf(TEXT("%.0f"), RoundedValue);
			}
			return FString::SanitizeFloat(NumberValue);
		}
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Null:
		return TEXT("");
	default:
		break;
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
	return Serialized;
}

static EBlueprintHelperAssetCollisionPolicy ParseAssetFactoryCollision(const FString& CollisionText)
{
	return CollisionText.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase)
		? EBlueprintHelperAssetCollisionPolicy::ReuseIfExists
		: EBlueprintHelperAssetCollisionPolicy::FailIfExists;
}

static void ApplyAssetFactoryResultData(
	FBlueprintHelperToolResultBase& Result,
	const FBlueprintHelperAssetFactoryData& FactoryData,
	const FString& AssetPath,
	EBlueprintHelperAssetType AssetType)
{
	Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("asset"));
	Result.Data = FactoryData.ToJson();

	if (Result.bOk && Result.Status == EBlueprintHelperToolStatus::Applied)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = FBlueprintHelperAssetFactoryService::ShouldCompile(AssetType);
		Validation.bShouldSave = FBlueprintHelperAssetFactoryService::ShouldSave(AssetType);
		Result.Validation = Validation;
	}
}

static TArray<FBlueprintHelperAssetFactoryFieldSpec> ReadAssetFactoryFieldsArray(
	const TSharedPtr<FJsonObject>& Payload)
{
	TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("fields"), Values) || !Values)
	{
		return Fields;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> FieldObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!FieldObject.IsValid())
		{
			continue;
		}

		FString Name;
		FString Type;
		if (!FieldObject->TryGetStringField(TEXT("name"), Name) ||
			!FieldObject->TryGetStringField(TEXT("type"), Type))
		{
			continue;
		}

		FBlueprintHelperAssetFactoryFieldSpec Field(Name, Type);
		if (FieldObject->HasField(TEXT("default_value")))
		{
			FString DefaultValue;
			if (!FieldObject->TryGetStringField(TEXT("default_value"), DefaultValue))
			{
				DefaultValue = JsonValueToString(FieldObject->TryGetField(TEXT("default_value")));
			}
			Field.DefaultValue = DefaultValue;
			Field.bHasDefaultValue = true;
		}
		Fields.Add(Field);
	}
	return Fields;
}

static TArray<FString> ReadTaskRuntimeStringArrayField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	TArray<FString> Result;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
	{
		return Result;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString Item;
		if (Value.IsValid() && Value->TryGetString(Item))
		{
			Result.Add(Item);
		}
	}
	return Result;
}

static TMap<FString, FString> ReadTaskRuntimeStringFieldsObject(const TSharedPtr<FJsonObject>& Payload)
{
	TMap<FString, FString> Fields;
	const TSharedPtr<FJsonObject>* FieldsObject = nullptr;
	if (!Payload.IsValid() ||
		!Payload->TryGetObjectField(TEXT("fields"), FieldsObject) ||
		!FieldsObject || !FieldsObject->IsValid())
	{
		return Fields;
	}

	for (const auto& Field : (*FieldsObject)->Values)
	{
		Fields.Add(FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key), JsonValueToString(Field.Value));
	}
	return Fields;
}

static TArray<FBlueprintHelperClassDefaultPropertySetting> ReadTaskRuntimeClassDefaultSettings(
	const TSharedPtr<FJsonObject>& Payload)
{
	TArray<FBlueprintHelperClassDefaultPropertySetting> Settings;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Values) || !Values)
	{
		return Settings;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			continue;
		}

		FBlueprintHelperClassDefaultPropertySetting Setting;
		Object->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
		Object->TryGetStringField(TEXT("mutation_strategy"), Setting.MutationStrategy);
		Setting.Value = Object->TryGetField(TEXT("value"));
		Settings.Add(MoveTemp(Setting));
	}
	return Settings;
}

static FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
	const TSharedPtr<FJsonObject>& Payload)
{
	FBlueprintHelperSetComponentPropertiesRequest Request;
	Request.Mode = EBlueprintHelperComponentPropertyMode::Batch;
	if (!Payload.IsValid())
	{
		return Request;
	}

	Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
	Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

	const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *SettingsArray)
		{
			const TSharedPtr<FJsonObject> ItemObject = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
			if (!ItemObject.IsValid())
			{
				continue;
			}

			FBlueprintHelperComponentPropertySetting Setting;
			ItemObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			Setting.Value = ItemObject->TryGetField(TEXT("value"));
			Request.Settings.Add(MoveTemp(Setting));
		}
	}
	return Request;
}

static TSharedRef<FJsonObject> MakeBlueprintVariableOpPayload(
	const FString& AssetPath,
	const FString& FunctionName,
	const TSharedPtr<FJsonObject>& OpObject)
{
	TSharedRef<FJsonObject> OpPayload = MakeShared<FJsonObject>();
	if (OpObject.IsValid())
	{
		for (const auto& Field : OpObject->Values)
		{
			const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key);
			if (Key != TEXT("op"))
			{
				OpPayload->SetField(Key, Field.Value);
			}
		}
	}
	OpPayload->SetStringField(TEXT("asset_path"), AssetPath);
	if (!FunctionName.IsEmpty())
	{
		OpPayload->SetStringField(TEXT("function_name"), FunctionName);
	}
	return OpPayload;
}

static FBlueprintHelperToolResultBase MakeWidgetMutationResult(
	const FString& Operation,
	const TSharedPtr<FJsonObject>& Payload,
	const FBlueprintHelperWidgetMutationResult& MutationResult)
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
		? (MutationResult.bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId)
			: FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId))
		: FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			TraceId,
			MakeTaskRuntimeError(
				TEXT("widget_operation_failed"),
				EBlueprintHelperToolStage::Execute,
				MutationResult.ErrorMessage));

	FString AssetPath;
	FString WidgetName;
	FString PropertyName;
	FString PropertyPath;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
		Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		Payload->TryGetStringField(TEXT("property_path"), PropertyPath);
		if (PropertyName.IsEmpty())
		{
			PropertyName = PropertyPath;
		}
	}
	Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("widget"), WidgetName, TEXT(""), PropertyName);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
	Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
	if (!MutationResult.AffectedWidget.IsEmpty())
	{
		Data->SetStringField(TEXT("widget_name"), MutationResult.AffectedWidget);
	}
	if (!PropertyName.IsEmpty())
	{
		Data->SetStringField(TEXT("property_name"), PropertyName);
	}
	if (!PropertyPath.IsEmpty())
	{
		Data->SetStringField(TEXT("property_path"), PropertyPath);
	}
	if (MutationResult.ReadbackContext.IsValid())
	{
		Data->SetObjectField(TEXT("readback_context"), MutationResult.ReadbackContext.ToSharedRef());
	}
	Result.Data = Data;
	return Result;
}

static FBlueprintHelperToolResultBase MakeDataTableMutationResult(
	const FString& Operation,
	const TSharedPtr<FJsonObject>& Payload,
	const FBlueprintHelperDataTableMutationResult& MutationResult)
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
		? (MutationResult.bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId)
			: FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId))
		: FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			TraceId,
			MakeTaskRuntimeError(
				TEXT("data_table_operation_failed"),
				EBlueprintHelperToolStage::Execute,
				MutationResult.ErrorMessage));

	FString AssetPath;
	FString RowName;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("row_name"), RowName);
	}
	Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("data_table_row"), TEXT(""), RowName);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("DataTableMutation.v1"));
	Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
	Data->SetStringField(TEXT("row_name"), MutationResult.AffectedRow.ToString());
	Result.Data = Data;
	return Result;
}

static bool TryBuildObjectPropertyRequest(
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperSetObjectPropertiesRequest& OutRequest,
	FString& OutError)
{
	OutRequest = FBlueprintHelperSetObjectPropertiesRequest();
	OutError.Reset();
	if (!Payload.IsValid())
	{
		OutError = TEXT("object_property adapter payload is required.");
		return false;
	}

	Payload->TryGetStringField(TEXT("asset_path"), OutRequest.AssetPath);
	Payload->TryGetBoolField(TEXT("dry_run"), OutRequest.bDryRun);
	if (OutRequest.AssetPath.IsEmpty())
	{
		OutError = TEXT("object_property adapter payload requires asset_path.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
	{
		for (const TSharedPtr<FJsonValue>& SettingValue : *SettingsArray)
		{
			const TSharedPtr<FJsonObject> SettingObject = SettingValue.IsValid()
				? SettingValue->AsObject()
				: nullptr;
			if (!SettingObject.IsValid())
			{
				OutError = TEXT("object_property settings entries must be objects.");
				return false;
			}

			FBlueprintHelperObjectPropertySetting Setting;
			SettingObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			if (Setting.PropertyPath.IsEmpty())
			{
				OutError = TEXT("object_property settings entries require property_path.");
				return false;
			}

			const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(SettingObject, TEXT("value"));
			if (!Value.IsValid())
			{
				OutError = TEXT("object_property settings entries require value.");
				return false;
			}

			Setting.Value = Value;
			OutRequest.Settings.Add(MoveTemp(Setting));
		}
		if (OutRequest.Settings.Num() == 0)
		{
			OutError = TEXT("object_property settings cannot be empty.");
			return false;
		}
		return true;
	}

	FBlueprintHelperObjectPropertySetting Setting;
	Payload->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
	if (Setting.PropertyPath.IsEmpty())
	{
		OutError = TEXT("object_property adapter payload requires property_path.");
		return false;
	}

	const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(Payload, TEXT("value"));
	if (!Value.IsValid())
	{
		OutError = TEXT("object_property adapter payload requires value.");
		return false;
	}

	Setting.Value = Value;
	OutRequest.Settings.Add(MoveTemp(Setting));
	return true;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::MakeFailure(
	const FString& Operation,
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
{
	return FBlueprintHelperToolResultBuilder::Failure(
		Operation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeTaskRuntimeError(Code, Stage, Message, Field));
}

static FBlueprintHelperToolResultBase MakeComponentPolicyParseFailure(
	const TCHAR* FieldName,
	const FString& Value)
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::MakeFailure(
		TEXT("blueprint_component"),
		TEXT("unsupported_blueprint_component_policy"),
		EBlueprintHelperToolStage::ParseInput,
		FString::Printf(TEXT("Unsupported blueprint component %s value: %s."), FieldName, *Value),
		TEXT("payload.") + FString(FieldName));
}

bool FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FString AssetPath;
	LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		LoweredStep.Payload->TryGetStringField(TEXT("blueprint_path"), AssetPath);
	}
	if (AssetPath.IsEmpty())
	{
		AssetPath = ReadTaskRuntimeReviewNestedStringField(LoweredStep.Payload, TEXT("target"), TEXT("asset_path"));
	}
	if (AssetPath.IsEmpty())
	{
		AssetPath = ReadTaskRuntimeReviewNestedStringField(LoweredStep.Payload, TEXT("target"), TEXT("blueprint_path"));
	}
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	OutEvidence.ArchiveSessionId = ArchiveSessionId;
	OutEvidence.TaskRunId = TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(TEXT("task_step_%s_%d"), *TaskRunId, StepIndex);
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = LoweredStep.AdapterOperation.IsEmpty()
		? LoweredStep.RuntimeOperation
		: LoweredStep.AdapterOperation;
	OutEvidence.DisplayLabel = OutEvidence.OperationKind;
	OutEvidence.ChangeKind = DeriveTaskRuntimeReviewChangeKind(OutEvidence.OperationKind);
	OutEvidence.TaskStepIndex = StepIndex;
	using FEvidencePredicate = TFunction<bool()>;
	using FEvidenceBuilder = TFunction<void()>;
	using FEvidenceRoute = TTuple<FEvidencePredicate, FEvidenceBuilder>;

	TArray<FEvidenceRoute> Routes;
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.AdapterOperation == FBlueprintHelperAssetFactoryTaskPlanAdapter::AdapterOperation;
		},
		[&OutEvidence, &LoweredStep]()
		{
			FString AssetType;
			LoweredStep.Payload->TryGetStringField(TEXT("asset_type"), AssetType);
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				FBlueprintHelperReviewTargetKindRegistry::ResolveAssetFactorySurface(AssetType),
				TEXT("asset_factory"),
				OutEvidence.AssetPath,
				TEXT("asset_factory"),
				AssetType.IsEmpty() ? OutEvidence.OperationKind : AssetType);

			if (FBlueprintHelperReviewTargetKindRegistry::IsStructureAssetType(AssetType))
			{
				AddTaskRuntimeReviewTargetsFromObjectArray(
					OutEvidence,
					LoweredStep.Payload,
					TEXT("fields"),
					TEXT("name"),
					EBlueprintHelperReviewSurface::DataAsset,
					TEXT("struct_field"),
					TEXT("struct_field"),
					TEXT("field"));
			}
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent;
		},
		[&OutEvidence, &LoweredStep]()
		{
			const FString ComponentName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("component_name"));
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::Components,
				TEXT("component"),
				ComponentName,
				TEXT("component"),
				ComponentName);
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperClassSettingsTaskPlanAdapter::CapabilityName;
		},
		[&OutEvidence, &LoweredStep]()
		{
			AddTaskRuntimeReviewTargetsFromStringArray(
				OutEvidence,
				LoweredStep.Payload,
				TEXT("interface_paths"),
				EBlueprintHelperReviewSurface::Details,
				TEXT("class_setting_interface"),
				TEXT("class_setting"),
				TEXT("interface"));
			AddTaskRuntimeClassDefaultReviewTargets(OutEvidence, LoweredStep.Payload);
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::Details,
				TEXT("class_setting_parent"),
				ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("new_parent_class")),
				TEXT("class_setting"),
				TEXT("parent class"));
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperSignatureTaskPlanAdapter::CapabilityName;
		},
		[&OutEvidence, &LoweredStep]()
		{
			FString SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("function_name"));
			if (SignatureName.IsEmpty())
			{
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("event_name"));
			}
			if (SignatureName.IsEmpty())
			{
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("macro_name"));
			}
			if (SignatureName.IsEmpty())
			{
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("dispatcher_name"));
			}
			if (SignatureName.IsEmpty())
			{
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("signature_name"));
			}
			FBlueprintHelperReviewAtomicTarget* Target = AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::MyBlueprint,
				TEXT("signature"),
				SignatureName,
				TEXT("signature"),
				SignatureName,
				ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("graph_name")));
			if (Target)
			{
				const FString SignatureSubKind = ResolveTaskRuntimeSignatureReviewSubKind(
					LoweredStep.AdapterOperation,
					LoweredStep.Payload);
				if (!SignatureSubKind.IsEmpty())
				{
					Target->TargetSubKind = SignatureSubKind;
					Target->VisualGroupKey = FString::Printf(
						TEXT("signature:%s:%s"),
						*SignatureSubKind,
						*MakeTaskRuntimeReviewRefSegment(SignatureName));
					Target->SignatureEvidenceId = Target->VisualGroupKey;
				}
			}
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperBlueprintVariableTaskPlanAdapter::CapabilityBlueprintVariable;
		},
		[&OutEvidence, &LoweredStep]()
		{
			AddTaskRuntimeReviewTargetsFromObjectArray(
				OutEvidence,
				LoweredStep.Payload,
				LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationAddMemberVariables
					? TEXT("variables")
					: TEXT("ops"),
				TEXT("name"),
				EBlueprintHelperReviewSurface::MyBlueprint,
				TEXT("blueprint_variable"),
				TEXT("variable"),
				TEXT("variable"));
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
		},
		[&OutEvidence, &LoweredStep]()
		{
			const FString WidgetName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("widget_name"));
			const FString PropertyName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("property_name"));
			const FString TargetName = PropertyName.IsEmpty()
				? WidgetName
				: FString::Printf(TEXT("%s.%s"), *WidgetName, *PropertyName);
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::UMGWidgetTree,
				PropertyName.IsEmpty() ? TEXT("umg_widget") : TEXT("umg_widget_property"),
				TargetName,
				TEXT("umg_widget"),
				TargetName);
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable;
		},
		[&OutEvidence, &LoweredStep]()
		{
			const FString RowName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("row_name"));
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::DataTable,
				TEXT("datatable_row"),
				RowName,
				TEXT("datatable"),
				RowName);
		}));
	Routes.Add(MakeTuple(
		[&LoweredStep]()
		{
			return LoweredStep.Capability == FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty;
		},
		[&OutEvidence, &LoweredStep]()
		{
			const FString PropertyPath = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("property_path"));
			if (!PropertyPath.IsEmpty())
			{
				AddTaskRuntimeReviewTarget(
					OutEvidence,
					LoweredStep.Payload,
					EBlueprintHelperReviewSurface::DataAsset,
					TEXT("object_property"),
					PropertyPath,
					TEXT("object_property"),
					PropertyPath);
			}
			AddTaskRuntimeReviewTargetsFromObjectArray(
				OutEvidence,
				LoweredStep.Payload,
				TEXT("settings"),
				TEXT("property_path"),
				EBlueprintHelperReviewSurface::DataAsset,
				TEXT("object_property"),
				TEXT("object_property"),
				TEXT("property"));
		}));

	for (const FEvidenceRoute& Route : Routes)
	{
		if (Route.Get<0>()())
		{
			Route.Get<1>()();
			break;
		}
	}

	return OutEvidence.AtomicTargets.Num() > 0;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteAssetFactoryTaskPlanStep(
	const FBlueprintHelperAssetFactoryService& Service,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	FString AssetTypeText;
	FString ParentClass;
	FString ValueType;
	FString RowStruct;
	FString DataAssetClass;
	FString CollisionText;
	TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("asset_type"), AssetTypeText);
		Payload->TryGetStringField(TEXT("parent_class"), ParentClass);
		Payload->TryGetStringField(TEXT("value_type"), ValueType);
		Payload->TryGetStringField(TEXT("row_struct"), RowStruct);
		Payload->TryGetStringField(TEXT("data_asset_class"), DataAssetClass);
		Payload->TryGetStringField(TEXT("collision"), CollisionText);
		Fields = ReadAssetFactoryFieldsArray(Payload);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}

	EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
	if (!FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(AssetTypeText, ParentClass, AssetType))
	{
		return MakeFailure(
			TEXT("create_asset"),
			TEXT("unsupported_asset_type"),
			EBlueprintHelperToolStage::ParseInput,
			FString::Printf(TEXT("Unsupported asset_type: %s"), *AssetTypeText),
			TEXT("task_plan.steps[0].write.ops[0].asset_type"));
	}

	if (AssetType == EBlueprintHelperAssetType::DataTable && RowStruct.TrimStartAndEnd().IsEmpty())
	{
		return MakeFailure(
			TEXT("create_asset"),
			TEXT("missing_row_struct"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_type=data_table requires row_struct."),
			TEXT("task_plan.steps[0].write.ops[0].row_struct"));
	}

	if (AssetType == EBlueprintHelperAssetType::BlueprintClass)
	{
		FString BlueprintParentError;
		if (!FBlueprintHelperAssetFactoryService::TryValidateBlueprintParentClass(ParentClass, BlueprintParentError))
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("invalid_blueprint_parent_class"),
				EBlueprintHelperToolStage::Preflight,
				BlueprintParentError,
				TEXT("task_plan.steps[0].write.ops[0].parent_class"));
		}
	}

	const EBlueprintHelperAssetCollisionPolicy Collision = ParseAssetFactoryCollision(CollisionText);
	const FBlueprintHelperAssetFactoryData FactoryData = Service.CreateAsset(
		AssetPath,
		AssetType,
		ParentClass,
		ValueType,
		RowStruct,
		DataAssetClass,
		Fields,
		Collision,
		bDryRun);

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result;
	if (FactoryData.Asset.bAlreadyExisted)
	{
		if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists &&
			FactoryData.Collision.bHandled)
		{
			Result = bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
				: FBlueprintHelperToolResultBuilder::NoOp(TEXT("create_asset"), TraceId);
		}
		else
		{
			Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("create_asset"),
				TraceId,
				MakeTaskRuntimeError(
					FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::FailIfExists
						? TEXT("asset_already_exists")
						: TEXT("asset_type_mismatch"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("Asset Factory could not create the requested asset.")));
		}
	}
	else if (!FactoryData.Asset.bCreated)
	{
		Result = bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
			: FBlueprintHelperToolResultBuilder::Failure(
				TEXT("create_asset"),
				TraceId,
				MakeTaskRuntimeError(
					TEXT("creation_failed"),
					EBlueprintHelperToolStage::Execute,
					TEXT("Failed to create asset.")));
	}
	else
	{
		Result = FBlueprintHelperToolResultBuilder::Applied(TEXT("create_asset"), TraceId);
	}

	ApplyAssetFactoryResultData(Result, FactoryData, AssetPath, AssetType);
	if (bDryRun && Result.Data.IsValid())
	{
		Result.Data->SetBoolField(TEXT("dry_run"), true);
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteBlueprintVariableBatchTaskPlanStep(
	const FBlueprintHelperBlueprintVariableService& Service,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	FString FunctionName;
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("function_name"), FunctionName);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}

	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	if (AssetPath.IsEmpty() ||
		!Payload.IsValid() ||
		!Payload->TryGetArrayField(TEXT("ops"), Ops) ||
		!Ops || Ops->Num() == 0)
	{
		return MakeFailure(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
			TEXT("invalid_variable_batch_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Blueprint variable batch payload requires asset_path and ops."),
			TEXT("task_plan.steps[0]"));
	}

	int32 AppliedCount = 0;
	int32 DryRunCount = 0;
	int32 NoOpCount = 0;
	for (int32 OpIndex = 0; OpIndex < Ops->Num(); ++OpIndex)
	{
		const TSharedPtr<FJsonObject> OpObject =
			(*Ops)[OpIndex].IsValid()
				? (*Ops)[OpIndex]->AsObject()
				: nullptr;
		if (!OpObject.IsValid())
		{
			return MakeFailure(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TEXT("invalid_variable_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable batch op must be an object."),
				FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex));
		}

		FString OpName;
		OpObject->TryGetStringField(TEXT("op"), OpName);
		const TSharedRef<FJsonObject> OpPayload = MakeBlueprintVariableOpPayload(AssetPath, FunctionName, OpObject);
		OpPayload->SetBoolField(TEXT("dry_run"), bDryRun);

		using FVariableOperationHandler = TFunction<FBlueprintHelperToolResultBase(const TSharedPtr<FJsonObject>&)>;
		TMap<FString, FVariableOperationHandler> OperationHandlers;
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.AddMemberVariable(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberVariableProperties,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.SetMemberVariableProperties(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.RemoveMemberVariable(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberDefault,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.SetMemberDefault(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureLocalVariable,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.AddLocalVariable(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetLocalVariableProperties,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.SetLocalVariableProperties(InPayload);
			});
		OperationHandlers.Add(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveLocalVariable,
			[&Service](const TSharedPtr<FJsonObject>& InPayload)
			{
				return Service.RemoveLocalVariable(InPayload);
			});

		const FVariableOperationHandler* Handler = OperationHandlers.Find(OpName);
		if (!Handler)
		{
			return MakeFailure(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TEXT("unsupported_variable_op"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("Unsupported blueprint variable op: %s."), *OpName),
				FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].op"), OpIndex));
		}

		const FBlueprintHelperToolResultBase OpResult = (*Handler)(OpPayload);
		if (!OpResult.bOk)
		{
			return OpResult;
		}

		if (OpResult.Status == EBlueprintHelperToolStatus::Applied)
		{
			++AppliedCount;
		}
		else if (OpResult.Status == EBlueprintHelperToolStatus::DryRun)
		{
			++DryRunCount;
		}
		else
		{
			++NoOpCount;
		}
	}

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = (bDryRun || DryRunCount > 0)
		? FBlueprintHelperToolResultBuilder::DryRun(
			FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
			TraceId)
		: (AppliedCount > 0
			? FBlueprintHelperToolResultBuilder::Applied(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId)
			: FBlueprintHelperToolResultBuilder::NoOp(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId));

	Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("blueprint"));
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.BlueprintVariableBatchResult.v1"));
	Data->SetNumberField(TEXT("requested_count"), Ops->Num());
	Data->SetNumberField(TEXT("applied_count"), AppliedCount);
	Data->SetNumberField(TEXT("dry_run_count"), DryRunCount);
	Data->SetNumberField(TEXT("no_op_count"), NoOpCount);
	Result.Data = Data;

	if (!bDryRun && AppliedCount > 0)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = true;
		Validation.bShouldSave = true;
		Result.Validation = Validation;
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteComponentTaskPlanStep(
	const FBlueprintHelperComponentService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	using FComponentOperationHandler = TFunction<FBlueprintHelperToolResultBase()>;
	TMap<FString, FComponentOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperAddComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("component_class"), Request.ComponentClass);
				Payload->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString NameCollisionPolicy;
				if (Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
				{
					if (!TryParseNameCollisionPolicy(NameCollisionPolicy, Request.NameCollisionPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("name_collision_policy"), NameCollisionPolicy);
					}
				}
			}
			return Service.AddComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties,
		[&Service, Payload]()
		{
			return Service.SetComponentProperties(ReadComponentPropertiesRequest(Payload));
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRenameComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperRenameComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("new_component_name"), Request.NewComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
			}
			return Service.RenameComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationReparentComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperReparentComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("new_parent_component"), Request.NewParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
			}
			return Service.ReparentComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAttachComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperAttachComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
			}
			return Service.AttachComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationDetachComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperDetachComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
				FString DefaultRootPolicy;
				if (Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
				{
					if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, Request.DefaultRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("default_root_policy"), DefaultRootPolicy);
					}
				}
			}
			return Service.DetachComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetRootComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperSetRootComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString OldRootPolicy;
				if (Payload->TryGetStringField(TEXT("old_root_policy"), OldRootPolicy))
				{
					if (!TryParseComponentOldRootPolicy(OldRootPolicy, Request.OldRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("old_root_policy"), OldRootPolicy);
					}
				}
				FString DefaultRootPolicy;
				if (Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
				{
					if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, Request.DefaultRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("default_root_policy"), DefaultRootPolicy);
					}
				}
			}
			return Service.SetRootComponent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperRemoveComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString DeletePolicy;
				if (Payload->TryGetStringField(TEXT("delete_policy"), DeletePolicy))
				{
					if (!TryParseComponentDeletePolicy(DeletePolicy, Request.DeletePolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("delete_policy"), DeletePolicy);
					}
				}
			}
			return Service.RemoveComponent(Request);
		});

	if (const FComponentOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return (*Handler)();
	}

	return MakeFailure(
		TEXT("blueprint_component"),
		TEXT("unsupported_component_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported component adapter operation."));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteClassSettingsTaskPlanStep(
	const FBlueprintHelperClassSettingsService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}

	using FClassSettingsOperationHandler = TFunction<FBlueprintHelperToolResultBase()>;
	TMap<FString, FClassSettingsOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperClassSettingsTaskPlanAdapter::AddImplementedInterfacesOp,
		[&Service, Payload, AssetPath, bDryRun]()
		{
			return Service.AddImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperClassSettingsTaskPlanAdapter::RemoveImplementedInterfacesOp,
		[&Service, Payload, AssetPath, bDryRun]()
		{
			return Service.RemoveImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperClassSettingsTaskPlanAdapter::SetClassDefaultPropertiesOp,
		[&Service, Payload, AssetPath, bDryRun]()
		{
			return Service.SetClassDefaultProperties(AssetPath, ReadTaskRuntimeClassDefaultSettings(Payload), bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperClassSettingsTaskPlanAdapter::ReparentBlueprintOp,
		[&Service, Payload, AssetPath, bDryRun]()
		{
			return Service.ReparentBlueprint(
				AssetPath,
				ReadTaskRuntimeReviewStringField(Payload, TEXT("new_parent_class")),
				bDryRun);
		});

	if (const FClassSettingsOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return (*Handler)();
	}

	return MakeFailure(
		TEXT("blueprint_class_settings"),
		TEXT("unsupported_class_settings_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported class settings adapter operation."));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteWidgetTaskPlanStep(
	const FBlueprintHelperWidgetService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	FString ParentName;
	FString WidgetClass;
	FString WidgetName;
	FString PropertyName;
	FString PropertyPath;
	FString Value;
	FString SlotName;
	FString HostWidgetName;
	FString ExpectedParentName;
	FString ExpectedContentWidgetName;
	FString ExpectedSlotClassPath;
	FString ExpectedWidgetClassPath;
	FString NewWidgetName;
	FString RootWidgetName;
	FString ReplacementPolicy;
	FString ReplacementWidgetClass;
	FString ReplacementWidgetName;
	FString ExpectedRootClassPath;
	FString NewParentClass;
	FString ExpectedParentClass;
	FString SourceWidgetName;
	FString TargetParentName;
	FString WrapperClass;
	FString WrapperName;
	FString NewWidgetClass;
	bool bDryRun = false;
	bool bReplaceExisting = false;
	bool bIsVariable = false;
	bool bPreserveChildren = true;
	bool bPreserveSlot = true;
	TOptional<int32> VirtualIndex;
	TOptional<int32> ExpectedVirtualIndex;
	TMap<FString, FString> NameMapping;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("parent_name"), ParentName);
		Payload->TryGetStringField(TEXT("new_parent_name"), ParentName);
		Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
		Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
		Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		Payload->TryGetStringField(TEXT("property_path"), PropertyPath);
		Payload->TryGetStringField(TEXT("value"), Value);
		Payload->TryGetStringField(TEXT("slot_name"), SlotName);
		Payload->TryGetStringField(TEXT("host_widget_name"), HostWidgetName);
		Payload->TryGetStringField(TEXT("expected_parent_name"), ExpectedParentName);
		Payload->TryGetStringField(TEXT("expected_content_widget_name"), ExpectedContentWidgetName);
		Payload->TryGetStringField(TEXT("expected_slot_class_path"), ExpectedSlotClassPath);
		Payload->TryGetStringField(TEXT("expected_widget_class_path"), ExpectedWidgetClassPath);
		Payload->TryGetStringField(TEXT("new_widget_name"), NewWidgetName);
		Payload->TryGetStringField(TEXT("root_widget_name"), RootWidgetName);
		Payload->TryGetStringField(TEXT("replacement_policy"), ReplacementPolicy);
		Payload->TryGetStringField(TEXT("replacement_widget_class"), ReplacementWidgetClass);
		Payload->TryGetStringField(TEXT("replacement_widget_name"), ReplacementWidgetName);
		Payload->TryGetStringField(TEXT("expected_root_class_path"), ExpectedRootClassPath);
		Payload->TryGetStringField(TEXT("new_parent_class"), NewParentClass);
		Payload->TryGetStringField(TEXT("expected_parent_class"), ExpectedParentClass);
		Payload->TryGetStringField(TEXT("source_widget_name"), SourceWidgetName);
		Payload->TryGetStringField(TEXT("target_parent_name"), TargetParentName);
		Payload->TryGetStringField(TEXT("wrapper_class"), WrapperClass);
		Payload->TryGetStringField(TEXT("wrapper_name"), WrapperName);
		Payload->TryGetStringField(TEXT("new_widget_class"), NewWidgetClass);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		Payload->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
		Payload->TryGetBoolField(TEXT("is_variable"), bIsVariable);
		Payload->TryGetBoolField(TEXT("preserve_children"), bPreserveChildren);
		Payload->TryGetBoolField(TEXT("preserve_slot"), bPreserveSlot);
		double NumberValue = 0.0;
		if (Payload->TryGetNumberField(TEXT("virtual_index"), NumberValue))
		{
			VirtualIndex = FMath::RoundToInt(NumberValue);
		}
		if (Payload->TryGetNumberField(TEXT("expected_virtual_index"), NumberValue))
		{
			ExpectedVirtualIndex = FMath::RoundToInt(NumberValue);
		}
		const TSharedPtr<FJsonObject>* NameMappingObject = nullptr;
		if (Payload->TryGetObjectField(TEXT("name_mapping"), NameMappingObject) &&
			NameMappingObject &&
			NameMappingObject->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*NameMappingObject)->Values)
			{
				if (Field.Value.IsValid() && Field.Value->Type == EJson::String)
				{
					NameMapping.Add(Field.Key, Field.Value->AsString());
				}
			}
		}
	}

	using FWidgetOperationHandler = TFunction<FBlueprintHelperWidgetMutationResult()>;
	TMap<FString, FWidgetOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget,
		[&Service, AssetPath, ParentName, SlotName, WidgetClass, WidgetName, ExpectedParentName, VirtualIndex, bDryRun]()
		{
			FBlueprintHelperAddWidgetRequest Request;
			Request.AssetPath = AssetPath;
			Request.ParentName = ParentName;
			Request.SlotName = SlotName;
			Request.WidgetClass = WidgetClass;
			Request.WidgetName = WidgetName;
			Request.VirtualIndex = VirtualIndex;
			Request.ExpectedParentName = ExpectedParentName;
			Request.bDryRun = bDryRun;
			return Service.AddWidget(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::MoveWidget,
		[&Service, AssetPath, ParentName, SlotName, WidgetName, ExpectedParentName, VirtualIndex, ExpectedVirtualIndex, bDryRun]()
		{
			FBlueprintHelperMoveWidgetRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.NewParentName = ParentName;
			Request.SlotName = SlotName;
			Request.VirtualIndex = VirtualIndex;
			Request.ExpectedParentName = ExpectedParentName;
			Request.ExpectedVirtualIndex = ExpectedVirtualIndex;
			Request.bDryRun = bDryRun;
			return Service.MoveWidget(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent,
		[&Service, AssetPath, HostWidgetName, SlotName, WidgetClass, WidgetName, ExpectedContentWidgetName, VirtualIndex, bReplaceExisting, bDryRun]()
		{
			FBlueprintHelperSetNamedSlotContentRequest Request;
			Request.AssetPath = AssetPath;
			Request.HostWidgetName = HostWidgetName;
			Request.SlotName = SlotName;
			Request.WidgetClass = WidgetClass;
			Request.WidgetName = WidgetName;
			Request.VirtualIndex = VirtualIndex;
			Request.ExpectedContentWidgetName = ExpectedContentWidgetName;
			Request.bReplaceExisting = bReplaceExisting;
			Request.bDryRun = bDryRun;
			return Service.SetNamedSlotContent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty,
		[&Service, AssetPath, WidgetName, PropertyName, Value, bDryRun]()
		{
			return Service.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value, bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty,
		[&Service, AssetPath, WidgetName, PropertyPath, Value, ExpectedSlotClassPath, bDryRun]()
		{
			FBlueprintHelperSetSlotPropertyRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.PropertyPath = PropertyPath;
			Request.Value = Value;
			Request.ExpectedSlotClassPath = ExpectedSlotClassPath;
			Request.bDryRun = bDryRun;
			return Service.SetSlotProperty(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable,
		[&Service, AssetPath, WidgetName, bIsVariable, ExpectedWidgetClassPath, bDryRun]()
		{
			FBlueprintHelperSetWidgetAsVariableRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.bIsVariable = bIsVariable;
			Request.ExpectedWidgetClassPath = ExpectedWidgetClassPath;
			Request.bDryRun = bDryRun;
			return Service.SetWidgetAsVariable(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget,
		[&Service, AssetPath, WidgetName, bDryRun]()
		{
			return Service.RemoveWidget(AssetPath, WidgetName, bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget,
		[&Service, AssetPath, WidgetName, NewWidgetName, ExpectedWidgetClassPath, bDryRun]()
		{
			FBlueprintHelperRenameWidgetRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.NewWidgetName = NewWidgetName;
			Request.ExpectedWidgetClassPath = ExpectedWidgetClassPath;
			Request.bDryRun = bDryRun;
			return Service.RenameWidget(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget,
		[&Service, AssetPath, RootWidgetName, ReplacementPolicy, ReplacementWidgetClass, ReplacementWidgetName, ExpectedRootClassPath, bDryRun]()
		{
			FBlueprintHelperRemoveRootWidgetRequest Request;
			Request.AssetPath = AssetPath;
			Request.RootWidgetName = RootWidgetName;
			Request.ReplacementPolicy = ReplacementPolicy;
			Request.ReplacementWidgetClass = ReplacementWidgetClass;
			Request.ReplacementWidgetName = ReplacementWidgetName;
			Request.ExpectedRootClassPath = ExpectedRootClassPath;
			Request.bDryRun = bDryRun;
			return Service.RemoveRootWidget(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReparentWidgetBlueprint,
		[&Service, AssetPath, NewParentClass, ExpectedParentClass, bDryRun]()
		{
			FBlueprintHelperReparentWidgetBlueprintRequest Request;
			Request.AssetPath = AssetPath;
			Request.NewParentClass = NewParentClass;
			Request.ExpectedParentClass = ExpectedParentClass;
			Request.bDryRun = bDryRun;
			return Service.ReparentWidgetBlueprint(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree,
		[&Service, AssetPath, SourceWidgetName, TargetParentName, SlotName, VirtualIndex, NameMapping, bDryRun]()
		{
			FBlueprintHelperDuplicateWidgetSubtreeRequest Request;
			Request.AssetPath = AssetPath;
			Request.SourceWidgetName = SourceWidgetName;
			Request.TargetParentName = TargetParentName;
			Request.SlotName = SlotName;
			Request.VirtualIndex = VirtualIndex;
			Request.NameMapping = NameMapping;
			Request.bDryRun = bDryRun;
			return Service.DuplicateWidgetSubtree(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget,
		[&Service, AssetPath, WidgetName, WrapperClass, WrapperName, bDryRun]()
		{
			FBlueprintHelperWrapWidgetRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.WrapperClass = WrapperClass;
			Request.WrapperName = WrapperName;
			Request.bDryRun = bDryRun;
			return Service.WrapWidget(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReplaceWidgetClass,
		[&Service, AssetPath, WidgetName, NewWidgetClass, ExpectedWidgetClassPath, bPreserveChildren, bPreserveSlot, bDryRun]()
		{
			FBlueprintHelperReplaceWidgetClassRequest Request;
			Request.AssetPath = AssetPath;
			Request.WidgetName = WidgetName;
			Request.NewWidgetClass = NewWidgetClass;
			Request.ExpectedWidgetClassPath = ExpectedWidgetClassPath;
			Request.bPreserveChildren = bPreserveChildren;
			Request.bPreserveSlot = bPreserveSlot;
			Request.bDryRun = bDryRun;
			return Service.ReplaceWidgetClass(Request);
		});

	if (const FWidgetOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return MakeWidgetMutationResult(AdapterOperation, Payload, (*Handler)());
	}

	return MakeFailure(
		TEXT("umg_widget"),
		TEXT("unsupported_widget_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported widget adapter operation."));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteDataTableTaskPlanStep(
	const FBlueprintHelperDataTableService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	FString RowName;
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("row_name"), RowName);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}

	using FDataTableOperationHandler = TFunction<FBlueprintHelperDataTableMutationResult()>;
	TMap<FString, FDataTableOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow,
		[&Service, Payload, AssetPath, RowName, bDryRun]()
		{
			return Service.AddDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow,
		[&Service, Payload, AssetPath, RowName, bDryRun]()
		{
			return Service.UpdateDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationDeleteRow,
		[&Service, AssetPath, RowName, bDryRun]()
		{
			return Service.DeleteDataTableRow(AssetPath, RowName, bDryRun);
		});

	if (const FDataTableOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return MakeDataTableMutationResult(AdapterOperation, Payload, (*Handler)());
	}

	return MakeFailure(
		TEXT("data_table"),
		TEXT("unsupported_data_table_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported DataTable adapter operation."));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteObjectPropertyTaskPlanStep(
	const FBlueprintHelperPropertyReflectionService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	FBlueprintHelperSetObjectPropertiesRequest Request;
	FString Error;
	if (!TryBuildObjectPropertyRequest(Payload, Request, Error))
	{
		return MakeFailure(
			TEXT("object_property"),
			TEXT("invalid_object_property_adapter_payload"),
			EBlueprintHelperToolStage::ParseInput,
			Error);
	}

	using FObjectPropertyOperationHandler = TFunction<FBlueprintHelperToolResultBase()>;
	TMap<FString, FObjectPropertyOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperty,
		[&Service, Request]()
		{
			return Service.SetObjectProperty(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperties,
		[&Service, Request]()
		{
			return Service.SetObjectProperties(Request);
		});

	if (const FObjectPropertyOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return (*Handler)();
	}

	return MakeFailure(
		TEXT("object_property"),
		TEXT("unsupported_object_property_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported object_property adapter operation."));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteSignatureTaskPlanStep(
	const FBlueprintHelperBlueprintStructureService& Service,
	const FString& AdapterOperation,
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AssetPath;
	FString FunctionName;
	FString EventName;
	FString MacroName;
	FString GraphName;
	FString DispatcherName;
	FString EventKind;
	FString SignatureKind;
	FString SignatureName;
	FString NameCollisionPolicy = TEXT("reuse_if_exists");
	FString InterfaceEntryKind;
	FString SignatureMismatchPolicy;
	FString ExecutePolicy;
	bool bIsPure = false;
	bool bDryRun = false;
	bool bRequireReferenceContext = true;
	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("function_name"), FunctionName);
		Payload->TryGetStringField(TEXT("event_name"), EventName);
		Payload->TryGetStringField(TEXT("macro_name"), MacroName);
		Payload->TryGetStringField(TEXT("graph_name"), GraphName);
		Payload->TryGetStringField(TEXT("dispatcher_name"), DispatcherName);
		Payload->TryGetStringField(TEXT("event_kind"), EventKind);
		Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind);
		Payload->TryGetStringField(TEXT("signature_name"), SignatureName);
		Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy);
		Payload->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind);
		Payload->TryGetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
		Payload->TryGetStringField(TEXT("execute_policy"), ExecutePolicy);
		Payload->TryGetBoolField(TEXT("is_pure"), bIsPure);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		Payload->TryGetBoolField(TEXT("require_reference_context"), bRequireReferenceContext);
		Payload->TryGetArrayField(TEXT("inputs"), Inputs);
		Payload->TryGetArrayField(TEXT("outputs"), Outputs);
	}

	const FBlueprintHelperSignatureService SignatureService(Service);
	using FSignatureOperationHandler = TFunction<FBlueprintHelperToolResultBase()>;
	TMap<FString, FSignatureOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureFunction,
		[&SignatureService, Payload, AssetPath, FunctionName, NameCollisionPolicy, SignatureMismatchPolicy, bDryRun, bIsPure, InterfaceEntryKind, Inputs, Outputs]()
		{
			FBlueprintHelperEnsureFunctionSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.FunctionName = FunctionName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			if (!SignatureMismatchPolicy.IsEmpty())
			{
				Request.SignatureMismatchPolicy = SignatureMismatchPolicy;
			}
			Request.bDryRun = bDryRun;
			Request.bIsPure = bIsPure;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			if (Outputs)
			{
				Request.Outputs = *Outputs;
			}
			return SignatureService.EnsureFunction(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureCustomEvent,
		[&SignatureService, Payload, AssetPath, GraphName, EventName, NameCollisionPolicy, bDryRun, InterfaceEntryKind, Inputs]()
		{
			FBlueprintHelperEnsureCustomEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			Request.bDryRun = bDryRun;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureCustomEvent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureMacro,
		[&SignatureService, AssetPath, MacroName, NameCollisionPolicy, bDryRun, Inputs, Outputs]()
		{
			FBlueprintHelperEnsureMacroSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.MacroName = MacroName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			if (Outputs)
			{
				Request.Outputs = *Outputs;
			}
			return SignatureService.EnsureMacro(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureEventDispatcher,
		[&SignatureService, AssetPath, DispatcherName, NameCollisionPolicy, SignatureMismatchPolicy, bDryRun, Inputs]()
		{
			FBlueprintHelperEnsureEventDispatcherSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.DispatcherName = DispatcherName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			if (!SignatureMismatchPolicy.IsEmpty())
			{
				Request.SignatureMismatchPolicy = SignatureMismatchPolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureEventDispatcher(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureOverrideEvent,
		[&SignatureService, AssetPath, GraphName, EventName, EventKind, ExecutePolicy, bDryRun, Inputs]()
		{
			FBlueprintHelperEnsureOverrideEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.EventKind = EventKind;
			if (!ExecutePolicy.IsEmpty())
			{
				Request.ExecutePolicy = ExecutePolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureOverrideEvent(Request);
		});
	OperationHandlers.Add(
		FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationRemoveSignature,
		[&SignatureService, AssetPath, GraphName, SignatureKind, SignatureName, ExecutePolicy, bDryRun, bRequireReferenceContext]()
		{
			FBlueprintHelperRemoveSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.SignatureKind = SignatureKind;
			Request.SignatureName = SignatureName;
			if (!ExecutePolicy.IsEmpty())
			{
				Request.ExecutePolicy = ExecutePolicy;
			}
			Request.bDryRun = bDryRun;
			Request.bRequireReferenceContext = bRequireReferenceContext;
			return SignatureService.RemoveSignature(Request);
		});

	if (const FSignatureOperationHandler* Handler = OperationHandlers.Find(AdapterOperation))
	{
		return (*Handler)();
	}

	return MakeFailure(
		TEXT("blueprint_signature"),
		TEXT("unsupported_signature_adapter_operation"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Unsupported signature adapter operation."));
}
