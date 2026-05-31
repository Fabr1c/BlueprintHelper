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

static FString MakeTaskRuntimeReviewExternalMergeBlockId(
	const FString& GraphName,
	const FString& InsertedBlockId)
{
	if (InsertedBlockId.IsEmpty())
	{
		return TEXT("");
	}
	if (GraphName.IsEmpty())
	{
		return InsertedBlockId;
	}

	const FString GraphPrefix = GraphName + TEXT("_");
	return InsertedBlockId.StartsWith(GraphPrefix)
		? InsertedBlockId
		: FString::Printf(TEXT("%s_%s"), *GraphName, *InsertedBlockId);
}

static void AddTaskRuntimeMergeExternalFlowReviewTargets(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return;
	}

	const FString GraphName = ReadTaskRuntimeReviewGraphName(Payload);
	const TSharedPtr<FJsonObject> Anchor = ReadTaskRuntimeReviewObjectField(Payload, TEXT("anchor"));
	const TSharedPtr<FJsonObject> Inserted = ReadTaskRuntimeReviewObjectField(Payload, TEXT("inserted"));
	if (GraphName.IsEmpty() || !Anchor.IsValid() || !Inserted.IsValid())
	{
		return;
	}

	FString NodeGuid;
	FString PinName;
	Anchor->TryGetStringField(TEXT("node_guid"), NodeGuid);
	Anchor->TryGetStringField(TEXT("pin_name"), PinName);
	if (NodeGuid.IsEmpty() || PinName.IsEmpty())
	{
		return;
	}

	const FString AnchorJson = SerializeTaskRuntimeReviewPayload(Anchor);
	const FString PayloadText = SerializeTaskRuntimeReviewPayload(Payload);
	const FString SafeGraphName = MakeTaskRuntimeReviewRefSegment(GraphName);
	const FString SafeNodeGuid = MakeTaskRuntimeReviewRefSegment(NodeGuid);
	const FString SafePinName = MakeTaskRuntimeReviewRefSegment(PinName);

	FBlueprintHelperReviewAtomicTarget BoundaryTarget;
	BoundaryTarget.AssetPath = Evidence.AssetPath;
	BoundaryTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	BoundaryTarget.GraphName = GraphName;
	BoundaryTarget.TargetKind = TEXT("graph_external_boundary");
	BoundaryTarget.TargetKey = FString::Printf(
		TEXT("graph_external_boundary:%s:node:%s:pin:%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafePinName);
	BoundaryTarget.VisualGroupKey = FString::Printf(
		TEXT("graph_external_boundary|%s|%s|%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafePinName);
	BoundaryTarget.DisplayLabel = FString::Printf(
		TEXT("External exec boundary %s.%s"),
		*GraphName,
		*PinName);
	BoundaryTarget.LatestEvidenceId = Evidence.EvidenceId;
	BoundaryTarget.SourceEvidenceIds.Add(Evidence.EvidenceId);
	BoundaryTarget.Ownership = TEXT("external_user_authored");
	BoundaryTarget.NodeGuid = NodeGuid;
	BoundaryTarget.PinPath = PinName;
	BoundaryTarget.AnchorJson = AnchorJson;
	BoundaryTarget.ExecutionOrder = Evidence.TaskStepIndex;
	BoundaryTarget.TaskStepIndex = Evidence.TaskStepIndex;
	BoundaryTarget.AtomicIndex = Evidence.AtomicTargets.Num();
	Evidence.AtomicTargets.Add(BoundaryTarget);

	FString InsertedBlockId;
	Inserted->TryGetStringField(TEXT("block_id"), InsertedBlockId);
	const FString FullBlockId = MakeTaskRuntimeReviewExternalMergeBlockId(GraphName, InsertedBlockId);
	if (FullBlockId.IsEmpty())
	{
		return;
	}

	const FString SafeBlockId = MakeTaskRuntimeReviewRefSegment(FullBlockId);
	FBlueprintHelperReviewAtomicTarget InsertedBlockTarget;
	InsertedBlockTarget.AssetPath = Evidence.AssetPath;
	InsertedBlockTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	InsertedBlockTarget.GraphName = GraphName;
	InsertedBlockTarget.TargetKind = TEXT("graph_block");
	InsertedBlockTarget.TargetKey = FString::Printf(TEXT("graph_block:block:%s"), *FullBlockId);
	InsertedBlockTarget.VisualGroupKey = FString::Printf(
		TEXT("graph_block:block:%s:%s"),
		*SafeGraphName,
		*SafeBlockId);
	InsertedBlockTarget.DisplayLabel = FString::Printf(
		TEXT("Inserted external flow %s"),
		*FullBlockId);
	InsertedBlockTarget.LatestEvidenceId = Evidence.EvidenceId;
	InsertedBlockTarget.SourceEvidenceIds.Add(Evidence.EvidenceId);
	InsertedBlockTarget.Ownership = TEXT("blueprinthelper_owned");
	InsertedBlockTarget.AnchorJson = PayloadText;
	InsertedBlockTarget.ExecutionOrder = Evidence.TaskStepIndex;
	InsertedBlockTarget.TaskStepIndex = Evidence.TaskStepIndex;
	InsertedBlockTarget.AtomicIndex = Evidence.AtomicTargets.Num();
	Evidence.AtomicTargets.Add(InsertedBlockTarget);
}

static void AddTaskRuntimePatchExternalGraphReviewTargets(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return;
	}

	const FString GraphName = ReadTaskRuntimeReviewGraphName(Payload);
	const TSharedPtr<FJsonObject> Anchor = ReadTaskRuntimeReviewObjectField(Payload, TEXT("anchor"));
	if (GraphName.IsEmpty() || !Anchor.IsValid())
	{
		return;
	}

	FString PatchType;
	FString NodeGuid;
	FString PinName;
	Payload->TryGetStringField(TEXT("patch_type"), PatchType);
	Anchor->TryGetStringField(TEXT("node_guid"), NodeGuid);
	Anchor->TryGetStringField(TEXT("pin_name"), PinName);
	if (PatchType.IsEmpty() || NodeGuid.IsEmpty())
	{
		return;
	}

	const FString FieldKind = PatchType == TEXT("set_external_pin_default")
		? TEXT("pin_default")
		: TEXT("node_comment");
	if (FieldKind == TEXT("pin_default") && PinName.IsEmpty())
	{
		return;
	}

	const FString AnchorJson = SerializeTaskRuntimeReviewPayload(Anchor);
	const FString SafeGraphName = MakeTaskRuntimeReviewRefSegment(GraphName);
	const FString SafeNodeGuid = MakeTaskRuntimeReviewRefSegment(NodeGuid);
	const FString SafeFieldKind = MakeTaskRuntimeReviewRefSegment(FieldKind);
	const FString SafePinName = MakeTaskRuntimeReviewRefSegment(PinName);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Evidence.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_external_node");
	Target.TargetKey = FieldKind == TEXT("pin_default")
		? FString::Printf(
			TEXT("graph_external_node:%s:node:%s:field:%s:pin:%s"),
			*SafeGraphName,
			*SafeNodeGuid,
			*SafeFieldKind,
			*SafePinName)
		: FString::Printf(
			TEXT("graph_external_node:%s:node:%s:field:%s"),
			*SafeGraphName,
			*SafeNodeGuid,
			*SafeFieldKind);
	Target.VisualGroupKey = FString::Printf(
		TEXT("graph_external_node|%s|%s|%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafeFieldKind);
	Target.DisplayLabel = FieldKind == TEXT("pin_default")
		? FString::Printf(TEXT("External pin default %s.%s"), *GraphName, *PinName)
		: FString::Printf(TEXT("External node comment %s"), *GraphName);
	Target.LatestEvidenceId = Evidence.EvidenceId;
	Target.SourceEvidenceIds.Add(Evidence.EvidenceId);
	Target.Ownership = TEXT("external_user_authored");
	Target.NodeGuid = NodeGuid;
	Target.PinPath = PinName;
	Target.PropertyPath = FieldKind;
	Target.AnchorJson = AnchorJson;
	Target.ExecutionOrder = Evidence.TaskStepIndex;
	Target.TaskStepIndex = Evidence.TaskStepIndex;
	Target.AtomicIndex = Evidence.AtomicTargets.Num();
	Evidence.AtomicTargets.Add(Target);
}

static void AddTaskRuntimeReplaceExternalBodyReviewTargets(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return;
	}

	const FString GraphName = ReadTaskRuntimeReviewGraphName(Payload);
	const TSharedPtr<FJsonObject> Anchor = ReadTaskRuntimeReviewObjectField(Payload, TEXT("anchor"));
	if (GraphName.IsEmpty() || !Anchor.IsValid())
	{
		return;
	}

	FString NodeGuid;
	Anchor->TryGetStringField(TEXT("node_guid"), NodeGuid);
	if (NodeGuid.IsEmpty())
	{
		return;
	}

	FString ReplaceScope;
	Payload->TryGetStringField(TEXT("scope"), ReplaceScope);
	if (ReplaceScope.IsEmpty())
	{
		ReplaceScope = ReadTaskRuntimeReviewNestedStringField(Payload, TEXT("target"), TEXT("replace_scope"));
	}
	if (ReplaceScope.IsEmpty())
	{
		return;
	}

	const FString AnchorJson = SerializeTaskRuntimeReviewPayload(Anchor);
	const FString SafeGraphName = MakeTaskRuntimeReviewRefSegment(GraphName);
	const FString SafeNodeGuid = MakeTaskRuntimeReviewRefSegment(NodeGuid);
	const FString SafeScope = MakeTaskRuntimeReviewRefSegment(ReplaceScope);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Evidence.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_external_body");
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_body:%s:node:%s:scope:%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafeScope);
	Target.VisualGroupKey = FString::Printf(
		TEXT("graph_external_body|%s|%s|%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafeScope);
	Target.DisplayLabel = FString::Printf(
		TEXT("External body %s %s"),
		*GraphName,
		*ReplaceScope);
	Target.LatestEvidenceId = Evidence.EvidenceId;
	Target.SourceEvidenceIds.Add(Evidence.EvidenceId);
	Target.Ownership = TEXT("external_user_authored");
	Target.NodeGuid = NodeGuid;
	Target.PropertyPath = ReplaceScope;
	Target.AnchorJson = AnchorJson;
	Target.ExecutionOrder = Evidence.TaskStepIndex;
	Target.TaskStepIndex = Evidence.TaskStepIndex;
	Target.AtomicIndex = Evidence.AtomicTargets.Num();
	Evidence.AtomicTargets.Add(Target);
}

static void AddTaskRuntimeReviewTarget(
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
		return;
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
	Evidence.AtomicTargets.Add(Target);
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

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*FieldsObject)->Values)
	{
		Fields.Add(Field.Key, JsonValueToString(Field.Value));
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
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : OpObject->Values)
		{
			if (Field.Key != TEXT("op"))
			{
				OpPayload->SetField(Field.Key, Field.Value);
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
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
		Payload->TryGetStringField(TEXT("property_name"), PropertyName);
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

			const TSharedPtr<FJsonValue>* Value = SettingObject->Values.Find(TEXT("value"));
			if (!Value || !Value->IsValid())
			{
				OutError = TEXT("object_property settings entries require value.");
				return false;
			}

			Setting.Value = *Value;
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

	const TSharedPtr<FJsonValue>* Value = Payload->Values.Find(TEXT("value"));
	if (!Value || !Value->IsValid())
	{
		OutError = TEXT("object_property adapter payload requires value.");
		return false;
	}

	Setting.Value = *Value;
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

	if (LoweredStep.AdapterOperation == TEXT("merge_external_flow"))
	{
		AddTaskRuntimeMergeExternalFlowReviewTargets(OutEvidence, LoweredStep.Payload);
		return OutEvidence.AtomicTargets.Num() > 0;
	}
	if (LoweredStep.AdapterOperation == TEXT("patch_external_graph"))
	{
		AddTaskRuntimePatchExternalGraphReviewTargets(OutEvidence, LoweredStep.Payload);
		return OutEvidence.AtomicTargets.Num() > 0;
	}
	if (LoweredStep.AdapterOperation == TEXT("replace_external_body"))
	{
		AddTaskRuntimeReplaceExternalBodyReviewTargets(OutEvidence, LoweredStep.Payload);
		return OutEvidence.AtomicTargets.Num() > 0;
	}

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
			AddTaskRuntimeReviewTargetsFromObjectArray(
				OutEvidence,
				LoweredStep.Payload,
				TEXT("settings"),
				TEXT("property_path"),
				EBlueprintHelperReviewSurface::Details,
				TEXT("class_default_property"),
				TEXT("class_setting"),
				TEXT("class default"));
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
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("dispatcher_name"));
			}
			if (SignatureName.IsEmpty())
			{
				SignatureName = ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("signature_name"));
			}
			AddTaskRuntimeReviewTarget(
				OutEvidence,
				LoweredStep.Payload,
				EBlueprintHelperReviewSurface::MyBlueprint,
				TEXT("signature"),
				SignatureName,
				TEXT("signature"),
				SignatureName,
				ReadTaskRuntimeReviewStringField(LoweredStep.Payload, TEXT("graph_name")));
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
					TryParseAttachRule(AttachRule, Request.AttachRule);
				}
				FString NameCollisionPolicy;
				if (Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
				{
					TryParseNameCollisionPolicy(NameCollisionPolicy, Request.NameCollisionPolicy);
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
		FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent,
		[&Service, Payload]()
		{
			FBlueprintHelperRemoveComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
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
	FString Value;
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		Payload->TryGetStringField(TEXT("parent_name"), ParentName);
		Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
		Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
		Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		Payload->TryGetStringField(TEXT("value"), Value);
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}

	using FWidgetOperationHandler = TFunction<FBlueprintHelperWidgetMutationResult()>;
	TMap<FString, FWidgetOperationHandler> OperationHandlers;
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget,
		[&Service, AssetPath, ParentName, WidgetClass, WidgetName, bDryRun]()
		{
			return Service.AddWidget(AssetPath, ParentName, WidgetClass, WidgetName, bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty,
		[&Service, AssetPath, WidgetName, PropertyName, Value, bDryRun]()
		{
			return Service.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value, bDryRun);
		});
	OperationHandlers.Add(
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget,
		[&Service, AssetPath, WidgetName, bDryRun]()
		{
			return Service.RemoveWidget(AssetPath, WidgetName, bDryRun);
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
		[&SignatureService, Payload, AssetPath, FunctionName, NameCollisionPolicy, bDryRun, bIsPure, InterfaceEntryKind, Inputs, Outputs]()
		{
			FBlueprintHelperEnsureFunctionSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.FunctionName = FunctionName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
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
