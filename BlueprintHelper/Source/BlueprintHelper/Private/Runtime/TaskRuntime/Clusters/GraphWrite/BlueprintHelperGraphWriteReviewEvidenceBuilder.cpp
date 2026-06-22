#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteReviewEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperK2GraphEntryEvidence.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperWriteReviewEvidenceProjection.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperExternalGraphWriteOperationPolicy.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipValidator.h"

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TrimmedPayloadString(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	FString Value;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadTargetStringField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* PrimaryFieldName,
	const TCHAR* AlternateFieldName)
{
	FString Value = TrimmedPayloadString(Payload, PrimaryFieldName);
	if (Value.IsEmpty() && AlternateFieldName)
	{
		Value = TrimmedPayloadString(Payload, AlternateFieldName);
	}
	if (!Value.IsEmpty() || !Payload.IsValid())
	{
		return Value;
	}

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		Value = TrimmedPayloadString(*TargetObject, PrimaryFieldName);
		if (Value.IsEmpty() && AlternateFieldName)
		{
			Value = TrimmedPayloadString(*TargetObject, AlternateFieldName);
		}
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadAssetPath(
	const TSharedPtr<FJsonObject>& Payload)
{
	return ReadTargetStringField(Payload, TEXT("asset_path"), TEXT("blueprint_path"));
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadGraphName(
	const TSharedPtr<FJsonObject>& Payload)
{
	return ReadTargetStringField(Payload, TEXT("graph_name"), TEXT("graph"));
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::SerializePayloadForAnchor(
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return FString();
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
	return Output;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::SerializeJsonObject(
	const TSharedRef<FJsonObject>& Object)
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object, Writer);
	return Output;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildGraphBodyBoundaryEvidence(
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel)
{
	return FBlueprintHelperGraphBodyBoundaryModelUtils::ToJsonObject(BoundaryModel);
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AugmentBoundaryModelFromStepResult(
	const FBlueprintHelperToolResultBase& StepResult,
	FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel)
{
	if (!StepResult.Data.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* BoundaryJson = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("graph_body_boundary"), BoundaryJson) &&
		BoundaryJson &&
		BoundaryJson->IsValid())
	{
		FString StringValue;
		if ((*BoundaryJson)->TryGetStringField(TEXT("runtime_adapter_id"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.RuntimeAdapterId = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("task_spec_strategy"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.TaskSpecStrategy = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("asset_path"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.TargetAssetPath = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("graph_name"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.GraphName = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("graph_family"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.GraphFamily = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("owned_block_id"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.OwnedBlockId = StringValue;
		}
		if ((*BoundaryJson)->TryGetStringField(TEXT("body_kind"), StringValue) && !StringValue.IsEmpty())
		{
			BoundaryModel.BodyKind = FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindFromString(StringValue);
		}
		AppendStringArrayField(*BoundaryJson, TEXT("entry_boundaries"), BoundaryModel.EntryNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("exit_boundaries"), BoundaryModel.ExitNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("protected_node_refs"), BoundaryModel.ProtectedNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("deletable_node_refs"), BoundaryModel.DeletableNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("generated_node_refs"), BoundaryModel.GeneratedNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("imported_body_node_refs"), BoundaryModel.ImportedBodyNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("reachable_body_flow_node_refs"), BoundaryModel.ReachableBodyFlowNodeRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("external_anchor_refs"), BoundaryModel.ExternalAnchorRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("semantic_source_refs"), BoundaryModel.SemanticSourceRefs);
		AppendStringArrayField(*BoundaryJson, TEXT("connectivity_exception_codes"), BoundaryModel.ConnectivityExceptionCodes);
	}

	FString RuntimeAdapterId;
	if (BoundaryModel.RuntimeAdapterId.IsEmpty() &&
		StepResult.Data->TryGetStringField(TEXT("runtime_adapter_id"), RuntimeAdapterId) &&
		!RuntimeAdapterId.IsEmpty())
	{
		BoundaryModel.RuntimeAdapterId = RuntimeAdapterId;
	}

	FString GraphBodyKind;
	if (BoundaryModel.BodyKind == EBlueprintHelperGraphBodyKind::Unknown &&
		StepResult.Data->TryGetStringField(TEXT("graph_body_kind"), GraphBodyKind) &&
		!GraphBodyKind.IsEmpty())
	{
		BoundaryModel.BodyKind = FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindFromString(GraphBodyKind);
	}
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TrimmedObjectStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadSignatureEvidenceId(
	const TSharedPtr<FJsonObject>& Payload)
{
	FString SignatureEvidenceId = TrimmedObjectStringField(Payload, TEXT("signature_evidence_id"));
	if (!SignatureEvidenceId.IsEmpty() || !Payload.IsValid())
	{
		return SignatureEvidenceId;
	}

	const TSharedPtr<FJsonObject>* LogicSpec = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpec) && LogicSpec && LogicSpec->IsValid())
	{
		SignatureEvidenceId = TrimmedObjectStringField(*LogicSpec, TEXT("signature_evidence_id"));
		if (!SignatureEvidenceId.IsEmpty())
		{
			return SignatureEvidenceId;
		}

		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if ((*LogicSpec)->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
		{
			SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
			if (!SignatureEvidenceId.IsEmpty())
			{
				return SignatureEvidenceId;
			}
		}
	}

	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (Payload->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
	{
		SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
	}
	return SignatureEvidenceId;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::ApplySignatureDependencyMetadata(
	FBlueprintHelperReviewAtomicTarget& Target,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FString& SignatureEvidenceId)
{
	if (SignatureEvidenceId.IsEmpty() || LoweredStep.DependsOn.Num() == 0)
	{
		return;
	}

	Target.SignatureRole = TEXT("dependency");
	Target.SignatureEvidenceId = SignatureEvidenceId;
	Target.DependencyOwnerStepId = LoweredStep.StepId;
	Target.DependentStepId = LoweredStep.DependsOn[0];
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		FString Text = Value->AsString();
		Text.TrimStartAndEndInline();
		if (!Text.IsEmpty())
		{
			OutValues.AddUnique(Text);
		}
	}
}

TArray<FString> FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadGraphBlockRefs(
	const FBlueprintHelperToolResultBase& StepResult)
{
	TArray<FString> BlockRefs;
	if (!StepResult.Data.IsValid())
	{
		return BlockRefs;
	}

	const TSharedPtr<FJsonObject>* AppendResult = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("append_result"), AppendResult) && AppendResult && AppendResult->IsValid())
	{
		AppendStringArrayField(*AppendResult, TEXT("block_refs"), BlockRefs);
	}
	AppendStringArrayField(StepResult.Data, TEXT("block_refs"), BlockRefs);

	return BlockRefs;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::MakeGraphBlockTargetKey(
	const FString& GraphName,
	const FString& BlockRef)
{
	const FString FullBlockId = BlockRef.StartsWith(GraphName + TEXT("_"))
		? BlockRef
		: FString::Printf(TEXT("%s_%s"), *GraphName, *BlockRef);
	return FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *FullBlockId);
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

TSharedPtr<FJsonObject> FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadObjectField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(FieldName, Child) && Child && Child->IsValid())
	{
		return *Child;
	}
	return nullptr;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadAnchorRefField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject> Anchor = ReadObjectField(Object, FieldName);
	return ReadStringField(Anchor, TEXT("anchor_ref"));
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::MakeReviewKeySegment(const FString& Value)
{
	FString Segment = Value;
	Segment.TrimStartAndEndInline();
	if (Segment.IsEmpty())
	{
		return TEXT("unknown");
	}

	for (TCHAR& Character : Segment)
	{
		const bool bAllowed =
			FChar::IsAlnum(Character) ||
			Character == TEXT('_') ||
			Character == TEXT('-') ||
			Character == TEXT('.');
		if (!bAllowed)
		{
			Character = TEXT('_');
		}
	}
	return Segment;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::MakeExternalLinkPatchAnchorRef(
	const TSharedPtr<FJsonObject>& Payload)
{
	FString AnchorRef = ReadAnchorRefField(Payload, TEXT("link_anchor"));
	if (!AnchorRef.IsEmpty())
	{
		return AnchorRef;
	}

	const FString SourceRef = ReadAnchorRefField(Payload, TEXT("source_anchor"));
	const FString TargetRef = ReadAnchorRefField(Payload, TEXT("target_anchor"));
	if (!SourceRef.IsEmpty() && !TargetRef.IsEmpty())
	{
		return SourceRef + TEXT(">") + TargetRef;
	}
	return FString();
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::MakeExternalMergeBlockId(
	const FString& GraphName,
	const FString& InsertedBlockId)
{
	if (InsertedBlockId.IsEmpty())
	{
		return FString();
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

static EBlueprintHelperK2GraphEntryKind BlueprintHelperGraphWriteReviewEntryKindForReplaceScope(
	const FString& ReplaceScope)
{
	if (ReplaceScope.Equals(TEXT("event_body"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperK2GraphEntryKind::Event;
	}
	if (ReplaceScope.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperK2GraphEntryKind::CustomEvent;
	}
	if (ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperK2GraphEntryKind::FunctionEntry;
	}
	if (ReplaceScope.Equals(TEXT("macro_body"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperK2GraphEntryKind::MacroEntry;
	}
	return EBlueprintHelperK2GraphEntryKind::Unknown;
}

static FString BlueprintHelperGraphWriteReviewReadStableEntryName(
	const TSharedPtr<FJsonObject>& Anchor,
	const FString& FallbackNodeGuid)
{
	static const TCHAR* StableNameFields[] =
	{
		TEXT("stable_name"),
		TEXT("entry_name"),
		TEXT("event_name"),
		TEXT("function_name"),
		TEXT("node_name")
	};

	for (const TCHAR* FieldName : StableNameFields)
	{
		FString Value;
		if (Anchor.IsValid() && Anchor->TryGetStringField(FieldName, Value))
		{
			Value.TrimStartAndEndInline();
			if (!Value.IsEmpty())
			{
				return Value;
			}
		}
	}
	return FallbackNodeGuid;
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildExternalMergeFlowEvidence(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& OperationKind,
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	const TSharedPtr<FJsonObject> Anchor = ReadObjectField(Input.LoweredStep.Payload, TEXT("anchor"));
	const TSharedPtr<FJsonObject> Inserted = ReadObjectField(Input.LoweredStep.Payload, TEXT("inserted"));
	if (!Anchor.IsValid() || !Inserted.IsValid())
	{
		return false;
	}

	const FString NodeGuid = ReadStringField(Anchor, TEXT("node_guid"));
	const FString PinName = ReadStringField(Anchor, TEXT("pin_name"));
	if (NodeGuid.IsEmpty() || PinName.IsEmpty())
	{
		return false;
	}

	const FString SafeGraphName = MakeReviewKeySegment(GraphName);
	const FString SafeNodeGuid = MakeReviewKeySegment(NodeGuid);
	const FString SafePinName = MakeReviewKeySegment(PinName);
	const FString BoundaryJson = SerializeJsonObject(BuildGraphBodyBoundaryEvidence(BoundaryModel));

	FBlueprintHelperReviewAtomicTarget BoundaryTarget;
	BoundaryTarget.AssetPath = AssetPath;
	BoundaryTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	BoundaryTarget.GraphName = GraphName;
	BoundaryTarget.TargetKind = TEXT("graph_external_boundary");
	BoundaryTarget.TargetKey = FString::Printf(
		TEXT("graph_external_boundary:%s:node:%s:pin:%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafePinName);
	BoundaryTarget.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, BoundaryTarget.TargetKey);
	BoundaryTarget.LifecycleObjectKey = BoundaryTarget.TargetKey;
	BoundaryTarget.VisualGroupKey = FString::Printf(
		TEXT("graph_external_boundary|%s|%s|%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafePinName);
	BoundaryTarget.DisplayLabel = FString::Printf(
		TEXT("External exec boundary %s.%s"),
		*GraphName,
		*PinName);
	BoundaryTarget.LatestEvidenceId = OutEvidence.EvidenceId;
	BoundaryTarget.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	BoundaryTarget.Ownership = TEXT("external_user_authored");
	BoundaryTarget.NodeGuid = NodeGuid;
	BoundaryTarget.PinPath = PinName;
	BoundaryTarget.AnchorJson = SerializeJsonObject(Anchor.ToSharedRef());
	BoundaryTarget.GraphBodyBoundaryJson = BoundaryJson;
	BoundaryTarget.ExecutionOrder = Input.StepIndex;
	BoundaryTarget.TaskStepIndex = Input.StepIndex;
	BoundaryTarget.AtomicIndex = OutEvidence.AtomicTargets.Num();
	OutEvidence.AtomicTargets.Add(BoundaryTarget);

	const FString InsertedBlockId = ReadStringField(Inserted, TEXT("block_id"));
	const FString FullBlockId = MakeExternalMergeBlockId(GraphName, InsertedBlockId);
	if (!FullBlockId.IsEmpty())
	{
		const FString SafeBlockId = MakeReviewKeySegment(FullBlockId);
		FBlueprintHelperReviewAtomicTarget InsertedBlockTarget;
		InsertedBlockTarget.AssetPath = AssetPath;
		InsertedBlockTarget.Surface = EBlueprintHelperReviewSurface::Graph;
		InsertedBlockTarget.GraphName = GraphName;
		InsertedBlockTarget.TargetKind = TEXT("graph_block");
		InsertedBlockTarget.TargetSubKind =
			FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(BoundaryModel.BodyKind);
		InsertedBlockTarget.TargetKey = FString::Printf(TEXT("graph_block:block:%s"), *FullBlockId);
		InsertedBlockTarget.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, InsertedBlockTarget.TargetKey);
		InsertedBlockTarget.LifecycleObjectKey = InsertedBlockTarget.TargetKey;
		InsertedBlockTarget.VisualGroupKey = FString::Printf(
			TEXT("graph_block:block:%s:%s"),
			*SafeGraphName,
			*SafeBlockId);
		InsertedBlockTarget.DisplayLabel = FString::Printf(
			TEXT("Inserted external flow %s"),
			*FullBlockId);
		InsertedBlockTarget.LatestEvidenceId = OutEvidence.EvidenceId;
		InsertedBlockTarget.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
		InsertedBlockTarget.Ownership = TEXT("blueprinthelper_owned");
		InsertedBlockTarget.AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);
		InsertedBlockTarget.GraphBodyBoundaryJson = BoundaryJson;
		InsertedBlockTarget.ExecutionOrder = Input.StepIndex;
		InsertedBlockTarget.TaskStepIndex = Input.StepIndex;
		InsertedBlockTarget.AtomicIndex = OutEvidence.AtomicTargets.Num();
		OutEvidence.AtomicTargets.Add(InsertedBlockTarget);
	}

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);
	return OutEvidence.AtomicTargets.Num() > 0;
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildExternalLinkPatchEvidence(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& OperationKind,
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	const FString PatchType = ReadStringField(Input.LoweredStep.Payload, TEXT("patch_type"));
	const FString AnchorRef = MakeExternalLinkPatchAnchorRef(Input.LoweredStep.Payload);
	if (PatchType.IsEmpty() || AnchorRef.IsEmpty())
	{
		return false;
	}

	const FString SafeGraphName = MakeReviewKeySegment(GraphName);
	const FString SafePatchType = MakeReviewKeySegment(PatchType);
	const FString SafeAnchorRef = MakeReviewKeySegment(AnchorRef);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_external_link");
	Target.TargetSubKind = PatchType;
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_link:%s:%s:%s"),
		*SafeGraphName,
		*SafePatchType,
		*SafeAnchorRef);
	Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
	Target.LifecycleObjectKey = Target.TargetKey;
	Target.VisualGroupKey = FString::Printf(
		TEXT("graph_external_link|%s|%s"),
		*SafeGraphName,
		*SafePatchType);
	Target.DisplayLabel = FString::Printf(TEXT("External link patch %s"), *PatchType);
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.Ownership = TEXT("external_user_authored");
	Target.PinPath = AnchorRef;
	Target.PropertyPath = PatchType;
	Target.AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);
	Target.GraphBodyBoundaryJson = SerializeJsonObject(BuildGraphBodyBoundaryEvidence(BoundaryModel));
	Target.ExecutionOrder = Input.StepIndex;
	Target.TaskStepIndex = Input.StepIndex;
	Target.AtomicIndex = OutEvidence.AtomicTargets.Num();
	OutEvidence.AtomicTargets.Add(Target);

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);
	return OutEvidence.AtomicTargets.Num() > 0;
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildExternalPropertyPatchEvidence(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& OperationKind,
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	const FString PatchType = ReadStringField(Input.LoweredStep.Payload, TEXT("patch_type"));
	const TSharedPtr<FJsonObject> Anchor = ReadObjectField(Input.LoweredStep.Payload, TEXT("anchor"));
	if (PatchType.IsEmpty() || !Anchor.IsValid())
	{
		return false;
	}

	FString NodeGuid;
	FString PinName;
	FString PropertyDescriptorId;
	Anchor->TryGetStringField(TEXT("node_guid"), NodeGuid);
	Anchor->TryGetStringField(TEXT("pin_name"), PinName);
	Input.LoweredStep.Payload->TryGetStringField(TEXT("property_descriptor_id"), PropertyDescriptorId);
	if ((NodeGuid.IsEmpty() || PinName.IsEmpty()) && Input.StepResult.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* ExternalPatch = nullptr;
		if (Input.StepResult.Data->TryGetObjectField(TEXT("external_patch"), ExternalPatch) &&
			ExternalPatch &&
			ExternalPatch->IsValid())
		{
			if (NodeGuid.IsEmpty())
			{
				(*ExternalPatch)->TryGetStringField(TEXT("node_guid"), NodeGuid);
			}
			if (PinName.IsEmpty())
			{
				(*ExternalPatch)->TryGetStringField(TEXT("pin_name"), PinName);
			}
		}
	}
	if (NodeGuid.IsEmpty())
	{
		return false;
	}

	FString FieldKind;
	if (PatchType == TEXT("set_external_pin_default"))
	{
		FieldKind = TEXT("pin_default");
	}
	else if (PatchType == TEXT("set_external_node_property"))
	{
		FieldKind = PropertyDescriptorId.IsEmpty() ? TEXT("node_property") : PropertyDescriptorId;
	}
	else if (PatchType == TEXT("set_external_node_comment"))
	{
		FieldKind = TEXT("node_comment");
	}
	else
	{
		return false;
	}
	if (FieldKind == TEXT("pin_default") && PinName.IsEmpty())
	{
		return false;
	}

	const FString SafeGraphName = MakeReviewKeySegment(GraphName);
	const FString SafeNodeGuid = MakeReviewKeySegment(NodeGuid);
	const FString SafeFieldKind = MakeReviewKeySegment(FieldKind);
	const FString SafePinName = MakeReviewKeySegment(PinName);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_external_node");
	Target.TargetSubKind = PatchType;
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
	Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
	Target.LifecycleObjectKey = Target.TargetKey;
	Target.VisualGroupKey = FString::Printf(
		TEXT("graph_external_node|%s|%s|%s"),
		*SafeGraphName,
		*SafeNodeGuid,
		*SafeFieldKind);
	Target.DisplayLabel = FieldKind == TEXT("pin_default")
		? FString::Printf(TEXT("External pin default %s.%s"), *GraphName, *PinName)
		: (PatchType == TEXT("set_external_node_property")
			? FString::Printf(TEXT("External node property %s %s"), *GraphName, *FieldKind)
			: FString::Printf(TEXT("External node comment %s"), *GraphName));
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.Ownership = TEXT("external_user_authored");
	Target.NodeGuid = NodeGuid;
	Target.PinPath = PinName;
	Target.PropertyPath = FieldKind;
	Target.AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);
	Target.GraphBodyBoundaryJson = SerializeJsonObject(BuildGraphBodyBoundaryEvidence(BoundaryModel));
	Target.ExecutionOrder = Input.StepIndex;
	Target.TaskStepIndex = Input.StepIndex;
	Target.AtomicIndex = OutEvidence.AtomicTargets.Num();
	OutEvidence.AtomicTargets.Add(Target);

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);
	return OutEvidence.AtomicTargets.Num() > 0;
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildExternalBodyReplaceEvidence(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& OperationKind,
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	const TSharedPtr<FJsonObject> Anchor = ReadObjectField(Input.LoweredStep.Payload, TEXT("anchor"));
	if (!Anchor.IsValid())
	{
		return false;
	}

	const FString NodeGuid = ReadStringField(Anchor, TEXT("node_guid"));
	if (NodeGuid.IsEmpty())
	{
		return false;
	}
	const FString StableEntryName = BlueprintHelperGraphWriteReviewReadStableEntryName(Anchor, FString());
	if (StableEntryName.IsEmpty())
	{
		return false;
	}

	FString ReplaceScope = ReadStringField(Input.LoweredStep.Payload, TEXT("scope"));
	if (ReplaceScope.IsEmpty())
	{
		const TSharedPtr<FJsonObject> TargetObject = ReadObjectField(Input.LoweredStep.Payload, TEXT("target"));
		ReplaceScope = ReadStringField(TargetObject, TEXT("replace_scope"));
	}
	if (ReplaceScope.IsEmpty())
	{
		return false;
	}

	FBlueprintHelperK2GraphEntryEvidence EntryEvidence;
	EntryEvidence.AssetPath = AssetPath;
	EntryEvidence.GraphName = GraphName;
	EntryEvidence.OperationKind = OperationKind;
	EntryEvidence.EntryIdentity.Kind = BlueprintHelperGraphWriteReviewEntryKindForReplaceScope(ReplaceScope);
	EntryEvidence.EntryIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	EntryEvidence.EntryIdentity.NodeGuid = NodeGuid;
	EntryEvidence.EntryIdentity.NodeClass = ReadStringField(Anchor, TEXT("node_class"));
	EntryEvidence.EntryIdentity.StableName = StableEntryName;
	EntryEvidence.EntryIdentity.GraphName = GraphName;
	EntryEvidence.EntryIdentity.bValid =
		EntryEvidence.EntryIdentity.Kind != EBlueprintHelperK2GraphEntryKind::Unknown &&
		!EntryEvidence.EntryIdentity.StableName.IsEmpty();
	EntryEvidence.BodyEntryAnchorJson = SerializeJsonObject(Anchor.ToSharedRef());
	EntryEvidence.GraphBodyBoundaryJson = SerializeJsonObject(BuildGraphBodyBoundaryEvidence(BoundaryModel));
	EntryEvidence.TargetOwnership = TEXT("external_user_authored");

	if (!Input.StepResult.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject> BeforeSnapshot =
		ReadObjectField(Input.StepResult.Data, TEXT("external_body_before_snapshot"));
	const TSharedPtr<FJsonObject> AfterSnapshot =
		ReadObjectField(Input.StepResult.Data, TEXT("external_body_after_snapshot"));
	if (!BeforeSnapshot.IsValid() || !AfterSnapshot.IsValid())
	{
		return false;
	}
	EntryEvidence.BeforeBodyFingerprint = ReadStringField(BeforeSnapshot, TEXT("body_fingerprint"));
	EntryEvidence.AfterBodyFingerprint = ReadStringField(AfterSnapshot, TEXT("body_fingerprint"));
	if (EntryEvidence.BeforeBodyFingerprint.IsEmpty() || EntryEvidence.AfterBodyFingerprint.IsEmpty())
	{
		return false;
	}
	BeforeSnapshot->SetBoolField(TEXT("exists"), true);
	AfterSnapshot->SetBoolField(TEXT("exists"), true);
	EntryEvidence.BeforeBodySnapshotJson = SerializeJsonObject(BeforeSnapshot.ToSharedRef());
	EntryEvidence.AfterBodySnapshotJson = SerializeJsonObject(AfterSnapshot.ToSharedRef());

	FBlueprintHelperReviewAtomicTarget Target;
	FString ProjectError;
	if (!FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(
		EntryEvidence,
		Input.StepIndex,
		OutEvidence.AtomicTargets.Num(),
		Target,
		ProjectError))
	{
		return false;
	}
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.PropertyPath = ReplaceScope;
	OutEvidence.AtomicTargets.Add(Target);

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);
	return OutEvidence.AtomicTargets.Num() > 0;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::NormalizeGraphWriteDiagnostic(
	FBlueprintHelperDiagnosticItem& Item,
	const FString& DefaultGraphName)
{
	if (Item.GraphName.IsEmpty())
	{
		Item.GraphName = DefaultGraphName;
	}
	if (Item.NodeGuid.IsEmpty() && !Item.NodeId.IsEmpty())
	{
		Item.NodeGuid = Item.NodeId;
	}
	if (Item.NodeId.IsEmpty() && !Item.NodeGuid.IsEmpty())
	{
		Item.NodeId = Item.NodeGuid;
	}
	if (Item.NodeName.IsEmpty())
	{
		Item.NodeName = Item.NodeTitle;
	}
	if (Item.CompileDiagnosticCorrelationKey.IsEmpty())
	{
		Item.CompileDiagnosticCorrelationKey = BlueprintHelperDiagnosticCorrelationKey(Item);
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendDiagnosticsFromArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	TArray<FBlueprintHelperDiagnosticItem> Items;
	BlueprintHelperReadDiagnosticArrayField(Object, FieldName, Items);
	for (FBlueprintHelperDiagnosticItem& Item : Items)
	{
		NormalizeGraphWriteDiagnostic(Item, DefaultGraphName);
		OutDiagnostics.Add(MoveTemp(Item));
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendReadbackCorrelationFromObject(
	const TSharedPtr<FJsonObject>& Object,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	if (!Object.IsValid())
	{
		return;
	}

	FBlueprintHelperDiagnosticItem Item;
	Item.Severity = EBlueprintHelperDiagnosticSeverity::Info;
	Item.Code = ReadStringField(Object, TEXT("code"));
	if (Item.Code.IsEmpty())
	{
		Item.Code = TEXT("graphwrite_readback_correlation");
	}
	Item.Message = ReadStringField(Object, TEXT("message"));
	Item.GraphName = ReadStringField(Object, TEXT("graph_name"));
	Item.NodeGuid = ReadStringField(Object, TEXT("node_guid"));
	Item.NodeId = ReadStringField(Object, TEXT("node_id"));
	Item.NodeTitle = ReadStringField(Object, TEXT("node_title"));
	Item.NodeName = ReadStringField(Object, TEXT("node_name"));
	Item.NodeClass = ReadStringField(Object, TEXT("node_class"));
	Item.BlockRef = ReadStringField(Object, TEXT("block_ref"));
	Item.TargetKey = ReadStringField(Object, TEXT("target_key"));
	Item.CompileDiagnosticCorrelationKey = ReadStringField(Object, TEXT("compile_diagnostic_correlation_key"));
	Item.ErrorType = ReadStringField(Object, TEXT("error_type"));
	NormalizeGraphWriteDiagnostic(Item, DefaultGraphName);

	if (!Item.CompileDiagnosticCorrelationKey.IsEmpty() ||
		!Item.NodeGuid.IsEmpty() ||
		!Item.TargetKey.IsEmpty() ||
		!Item.BlockRef.IsEmpty())
	{
		OutDiagnostics.Add(MoveTemp(Item));
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendReadbackCorrelationsFromArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		AppendReadbackCorrelationFromObject(Value.IsValid() ? Value->AsObject() : nullptr, DefaultGraphName, OutDiagnostics);
	}
}

TArray<FBlueprintHelperDiagnosticItem> FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadReviewDiagnostics(
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& DefaultGraphName)
{
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	if (!StepResult.Data.IsValid())
	{
		return Diagnostics;
	}

	AppendDiagnosticsFromArrayField(StepResult.Data, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
	AppendDiagnosticsFromArrayField(StepResult.Data, TEXT("compiler_results"), DefaultGraphName, Diagnostics);

	const TSharedPtr<FJsonObject>* CompileResult = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("compile_result"), CompileResult) && CompileResult && CompileResult->IsValid())
	{
		AppendDiagnosticsFromArrayField(*CompileResult, TEXT("compiler_results"), DefaultGraphName, Diagnostics);
		AppendDiagnosticsFromArrayField(*CompileResult, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
	}

	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("generated_nodes"), DefaultGraphName, Diagnostics);
	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("readback_nodes"), DefaultGraphName, Diagnostics);
	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("readback_entries"), DefaultGraphName, Diagnostics);

	const TSharedPtr<FJsonObject>* Readback = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("readback"), Readback) && Readback && Readback->IsValid())
	{
		AppendDiagnosticsFromArrayField(*Readback, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationsFromArrayField(*Readback, TEXT("generated_nodes"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationsFromArrayField(*Readback, TEXT("nodes"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationFromObject(*Readback, DefaultGraphName, Diagnostics);
	}

	return Diagnostics;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TargetKeyForDiagnostic(
	const FBlueprintHelperDiagnosticItem& Diagnostic,
	const FString& DefaultGraphName)
{
	if (!Diagnostic.TargetKey.IsEmpty())
	{
		return Diagnostic.TargetKey;
	}
	const FString GraphName = Diagnostic.GraphName.IsEmpty() ? DefaultGraphName : Diagnostic.GraphName;
	if (!GraphName.IsEmpty() && !Diagnostic.BlockRef.IsEmpty())
	{
		return MakeGraphBlockTargetKey(GraphName, Diagnostic.BlockRef);
	}
	return FString();
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::DiagnosticMatchesTarget(
	const FBlueprintHelperDiagnosticItem& Diagnostic,
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	const FString DiagnosticTargetKey = TargetKeyForDiagnostic(Diagnostic, Target.GraphName);
	if (!DiagnosticTargetKey.IsEmpty())
	{
		return DiagnosticTargetKey == Target.TargetKey;
	}

	const bool bDiagnosticHasNodeIdentity =
		!Diagnostic.NodeGuid.IsEmpty() ||
		!Diagnostic.CompileDiagnosticCorrelationKey.IsEmpty();
	if (bDiagnosticHasNodeIdentity)
	{
		return !Diagnostic.NodeGuid.IsEmpty() &&
			!Target.NodeGuid.IsEmpty() &&
			Diagnostic.NodeGuid.Equals(Target.NodeGuid, ESearchCase::IgnoreCase) &&
			(Diagnostic.GraphName.IsEmpty() || Diagnostic.GraphName == Target.GraphName);
	}

	return Diagnostic.GraphName.IsEmpty() || Diagnostic.GraphName == Target.GraphName;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AttachDiagnosticsToEvidence(
	const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	OutEvidence.Diagnostics = Diagnostics;
	for (FBlueprintHelperReviewAtomicTarget& Target : OutEvidence.AtomicTargets)
	{
		for (FBlueprintHelperDiagnosticItem Diagnostic : Diagnostics)
		{
			NormalizeGraphWriteDiagnostic(Diagnostic, Target.GraphName);
			if (Diagnostic.TargetKey.IsEmpty())
			{
				Diagnostic.TargetKey = TargetKeyForDiagnostic(Diagnostic, Target.GraphName);
			}
			if (DiagnosticMatchesTarget(Diagnostic, Target))
			{
				Target.Diagnostics.Add(MoveTemp(Diagnostic));
			}
		}
	}
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildMaterialGraphEvidence(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& OperationKind,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!Input.StepResult.Data.IsValid())
	{
		return false;
	}

	OutEvidence.DisplayLabel = TEXT("MaterialGraph edit");
	OutEvidence.BeforeSummary = TEXT("MaterialGraph before write");
	OutEvidence.AfterSummary = TEXT("MaterialGraph after write");
	const FString AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);

	struct FMaterialGraphReviewArray
	{
		const TCHAR* FieldName;
		const TCHAR* TargetKind;
		const TCHAR* TargetSubKind;
		EBlueprintHelperReviewChangeKind ChangeKind;
	};
	const FMaterialGraphReviewArray Arrays[] =
	{
		{ TEXT("created_expression_refs"), TEXT("material_expression"), TEXT("created_expression"), EBlueprintHelperReviewChangeKind::Added },
		{ TEXT("updated_property_refs"), TEXT("material_expression"), TEXT("updated_properties"), EBlueprintHelperReviewChangeKind::Modified },
		{ TEXT("deleted_expression_refs"), TEXT("material_expression"), TEXT("deleted_expression"), EBlueprintHelperReviewChangeKind::Removed }
	};

	for (const FMaterialGraphReviewArray& Array : Arrays)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Input.StepResult.Data->TryGetArrayField(Array.FieldName, Values) || !Values)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			const FString NodeKey = ReadStringField(Object, TEXT("node_key"));
			const FString BlockId = ReadStringField(Object, TEXT("block_id"));
			const FString ExpressionGuid = ReadStringField(Object, TEXT("expression_guid"));
			const FString ClassName = ReadStringField(Object, TEXT("class_name"));
			const FString Selector = ReadStringField(Object, TEXT("selector"));
			const FString TargetKey = FString::Printf(TEXT("material_expression:%s"), *MakeReviewKeySegment(NodeKey));

			FBlueprintHelperReviewAtomicTarget Target;
			Target.AssetPath = AssetPath;
			Target.Surface = EBlueprintHelperReviewSurface::Material;
			Target.GraphName = GraphName;
			Target.TargetKind = Array.TargetKind;
			Target.TargetSubKind = Array.TargetSubKind;
			Target.TargetKey = TargetKey;
			Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
			Target.LifecycleObjectKey = Target.TargetKey;
			Target.VisualGroupKey = FString::Printf(TEXT("material_graph|%s|%s"), *GraphName, *MakeReviewKeySegment(BlockId));
			Target.DisplayLabel = NodeKey.IsEmpty() ? FString(Array.TargetSubKind) : NodeKey;
			Target.LatestEvidenceId = OutEvidence.EvidenceId;
			Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
			Target.Ownership = TEXT("blueprinthelper_owned");
			Target.NodeGuid = ExpressionGuid;
			Target.PropertyPath = Selector.IsEmpty() ? ClassName : Selector;
			Target.ComponentPath = BlockId;
			Target.AnchorJson = AnchorJson;
			const FString SnapshotJson = SerializeJsonObject(Object.ToSharedRef());
			if (Array.ChangeKind == EBlueprintHelperReviewChangeKind::Removed)
			{
				Target.BeforeSnapshotJson = SnapshotJson;
			}
			else if (Array.ChangeKind == EBlueprintHelperReviewChangeKind::Added)
			{
				TSharedRef<FJsonObject> MissingBefore = MakeShared<FJsonObject>();
				MissingBefore->SetBoolField(TEXT("exists"), false);
				MissingBefore->SetStringField(TEXT("target_kind"), Array.TargetKind);
				if (!NodeKey.IsEmpty())
				{
					MissingBefore->SetStringField(TEXT("node_key"), NodeKey);
				}
				if (!BlockId.IsEmpty())
				{
					MissingBefore->SetStringField(TEXT("block_id"), BlockId);
				}
				if (!ExpressionGuid.IsEmpty())
				{
					MissingBefore->SetStringField(TEXT("expression_guid"), ExpressionGuid);
				}
				Target.BeforeSnapshotJson = SerializeJsonObject(MissingBefore);
				Target.AfterSnapshotJson = SnapshotJson;
			}
			else if (Array.ChangeKind == EBlueprintHelperReviewChangeKind::Modified)
			{
				const TSharedPtr<FJsonObject>* BeforeObject = nullptr;
				const TSharedPtr<FJsonObject>* AfterObject = nullptr;
				if (Object->TryGetObjectField(TEXT("before"), BeforeObject) && BeforeObject && BeforeObject->IsValid())
				{
					(*BeforeObject)->SetBoolField(TEXT("exists"), true);
					(*BeforeObject)->SetStringField(TEXT("target_kind"), Array.TargetKind);
					Target.BeforeSnapshotJson = SerializeJsonObject((*BeforeObject).ToSharedRef());
				}
				if (Object->TryGetObjectField(TEXT("after"), AfterObject) && AfterObject && AfterObject->IsValid())
				{
					(*AfterObject)->SetBoolField(TEXT("exists"), true);
					(*AfterObject)->SetStringField(TEXT("target_kind"), Array.TargetKind);
					Target.AfterSnapshotJson = SerializeJsonObject((*AfterObject).ToSharedRef());
				}
				if (Target.AfterSnapshotJson.IsEmpty())
				{
					Target.AfterSnapshotJson = SnapshotJson;
				}
			}
			else
			{
				Target.AfterSnapshotJson = SnapshotJson;
			}
			Target.ExecutionOrder = Input.StepIndex;
			Target.TaskStepIndex = Input.StepIndex;
			Target.AtomicIndex = OutEvidence.AtomicTargets.Num();
			OutEvidence.AtomicTargets.Add(Target);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Connections = nullptr;
	if (Input.StepResult.Data->TryGetArrayField(TEXT("connections"), Connections) && Connections)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Connections)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			const FString FromNodeKey = ReadStringField(Object, TEXT("from_node_key"));
			const FString FromPin = ReadStringField(Object, TEXT("from_pin"));
			const FString ToNodeKey = ReadStringField(Object, TEXT("to_node_key"));
			const FString ToPin = ReadStringField(Object, TEXT("to_pin"));
			FBlueprintHelperReviewAtomicTarget Target;
			Target.AssetPath = AssetPath;
			Target.Surface = EBlueprintHelperReviewSurface::Material;
			Target.GraphName = GraphName;
			Target.TargetKind = ToNodeKey == TEXT("$material_output") ? TEXT("material_output_link") : TEXT("material_expression_link");
			Target.TargetSubKind = TEXT("connection");
			Target.TargetKey = FString::Printf(
				TEXT("material_link:%s:%s:%s:%s"),
				*MakeReviewKeySegment(FromNodeKey),
				*MakeReviewKeySegment(FromPin),
				*MakeReviewKeySegment(ToNodeKey),
				*MakeReviewKeySegment(ToPin));
			Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
			Target.LifecycleObjectKey = Target.TargetKey;
			Target.VisualGroupKey = FString::Printf(TEXT("material_graph|%s|links"), *GraphName);
			Target.DisplayLabel = FString::Printf(TEXT("%s.%s -> %s.%s"), *FromNodeKey, *FromPin, *ToNodeKey, *ToPin);
			Target.LatestEvidenceId = OutEvidence.EvidenceId;
			Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
			Target.Ownership = TEXT("blueprinthelper_owned");
			Target.PinPath = FString::Printf(TEXT("%s.%s>%s.%s"), *FromNodeKey, *FromPin, *ToNodeKey, *ToPin);
			Target.PropertyPath = ToPin;
			Target.AnchorJson = AnchorJson;
			TSharedRef<FJsonObject> MissingBefore = MakeShared<FJsonObject>();
			MissingBefore->SetBoolField(TEXT("exists"), false);
			MissingBefore->SetStringField(TEXT("target_kind"), Target.TargetKind);
			MissingBefore->SetStringField(TEXT("from_node_key"), FromNodeKey);
			MissingBefore->SetStringField(TEXT("from_pin"), FromPin);
			MissingBefore->SetStringField(TEXT("to_node_key"), ToNodeKey);
			MissingBefore->SetStringField(TEXT("to_pin"), ToPin);
			Target.BeforeSnapshotJson = SerializeJsonObject(MissingBefore);
			Target.AfterSnapshotJson = SerializeJsonObject(Object.ToSharedRef());
			Target.ExecutionOrder = Input.StepIndex;
			Target.TaskStepIndex = Input.StepIndex;
			Target.AtomicIndex = OutEvidence.AtomicTargets.Num();
			OutEvidence.AtomicTargets.Add(Target);
		}
	}

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics = ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);
	return OutEvidence.AtomicTargets.Num() > 0;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildScopeIdentity(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& TargetKey)
{
	if (AssetPath.IsEmpty() || GraphName.IsEmpty() || TargetKey.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s|%s|%s"), *AssetPath, *GraphName, *TargetKey);
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::Build(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!Input.StepResult.bOk || !Input.LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel = Input.BoundaryModel;
	AugmentBoundaryModelFromStepResult(Input.StepResult, BoundaryModel);

	FString AssetPath = BoundaryModel.TargetAssetPath;
	if (AssetPath.IsEmpty())
	{
		AssetPath = ReadAssetPath(Input.LoweredStep.Payload);
		BoundaryModel.TargetAssetPath = AssetPath;
	}
	FString GraphName = BoundaryModel.GraphName;
	if (GraphName.IsEmpty())
	{
		GraphName = ReadGraphName(Input.LoweredStep.Payload);
		BoundaryModel.GraphName = GraphName;
	}

	const FString OperationKind = Input.LoweredStep.AdapterOperation.IsEmpty()
		? Input.LoweredStep.RuntimeOperation
		: Input.LoweredStep.AdapterOperation;
	if (GraphName.IsEmpty() && OperationKind == TEXT("material_graph_edit"))
	{
		GraphName = TEXT("MaterialGraph");
		BoundaryModel.GraphName = GraphName;
	}
	if (AssetPath.IsEmpty() || GraphName.IsEmpty())
	{
		return false;
	}

	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	OutEvidence.ArchiveSessionId = Input.ArchiveSessionId;
	OutEvidence.TaskRunId = Input.TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(TEXT("task_step_%s_%d"), *Input.TaskRunId, Input.StepIndex);
	OutEvidence.CreatedAt = FDateTime::UtcNow().ToIso8601();
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = OperationKind;
	OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	OutEvidence.DisplayLabel = OperationKind;
	OutEvidence.TaskStepIndex = Input.StepIndex;
	if (OperationKind == TEXT("material_graph_edit"))
	{
		return BuildMaterialGraphEvidence(
			Input,
			AssetPath,
			GraphName,
			OperationKind,
			OutEvidence);
	}

	EBlueprintHelperExternalGraphWriteAdapterOperationKind ExternalOperationKind =
		EBlueprintHelperExternalGraphWriteAdapterOperationKind::Unknown;
	if (FBlueprintHelperExternalGraphWriteOperationPolicy::TryClassifyAdapterOperation(
		OperationKind,
		ExternalOperationKind))
	{
		if (ExternalOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::MergeFlow)
		{
			return BuildExternalMergeFlowEvidence(
				Input,
				AssetPath,
				GraphName,
				OperationKind,
				BoundaryModel,
				OutEvidence);
		}
		if (ExternalOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::PropertyPatch)
		{
			return BuildExternalPropertyPatchEvidence(
				Input,
				AssetPath,
				GraphName,
				OperationKind,
				BoundaryModel,
				OutEvidence);
		}
		if (ExternalOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::LinkPatch)
		{
			return BuildExternalLinkPatchEvidence(
				Input,
				AssetPath,
				GraphName,
				OperationKind,
				BoundaryModel,
				OutEvidence);
		}
		if (ExternalOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::BodyReplace)
		{
			return BuildExternalBodyReplaceEvidence(
				Input,
				AssetPath,
				GraphName,
				OperationKind,
				BoundaryModel,
				OutEvidence);
		}
	}

	const TArray<FString> BlockRefs = ReadGraphBlockRefs(Input.StepResult);
	TArray<FString> TargetKeys;
	for (const FString& BlockRef : BlockRefs)
	{
		TargetKeys.AddUnique(MakeGraphBlockTargetKey(GraphName, BlockRef));
	}
	if (TargetKeys.Num() == 0)
	{
		TargetKeys.Add(FString::Printf(TEXT("graph_block:%s"), *GraphName));
	}

	const FString AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);
	const FString SignatureEvidenceId = ReadSignatureEvidenceId(Input.LoweredStep.Payload);
	for (int32 TargetIndex = 0; TargetIndex < TargetKeys.Num(); ++TargetIndex)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = GraphName;
		Target.TargetKind = TEXT("graph_block");
		Target.TargetSubKind = FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(BoundaryModel.BodyKind);
		Target.TargetKey = TargetKeys[TargetIndex];
		Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
		Target.LifecycleObjectKey = Target.TargetKey;
		Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
		Target.DisplayLabel = FString::Printf(TEXT("%s %s"), *OperationKind, *GraphName);
		Target.LatestEvidenceId = OutEvidence.EvidenceId;
		Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
		Target.Ownership = TEXT("graph_write");
		Target.AnchorJson = AnchorJson;
		Target.GraphBodyBoundaryJson = SerializeJsonObject(BuildGraphBodyBoundaryEvidence(BoundaryModel));
		Target.ExecutionOrder = Input.StepIndex;
		Target.TaskStepIndex = Input.StepIndex;
		Target.AtomicIndex = TargetIndex;
		FBlueprintHelperWriteReviewEvidenceProjection::ApplyBoundaryToAtomicTarget(
			FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(Target),
			Target);
		ApplySignatureDependencyMetadata(
			Target,
			Input.LoweredStep,
			SignatureEvidenceId);
		OutEvidence.AtomicTargets.Add(Target);
	}

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);

	FBlueprintHelperGraphWriteOwnershipValidationInput OwnershipValidationInput;
	OwnershipValidationInput.GeneratedBlockRefs = BlockRefs;
	OwnershipValidationInput.AtomicTargets = OutEvidence.AtomicTargets;
	const FBlueprintHelperGraphWriteOwnershipValidationResult OwnershipValidation =
		FBlueprintHelperGraphWriteOwnershipValidator::Validate(OwnershipValidationInput);
	return OutEvidence.AtomicTargets.Num() > 0 && OwnershipValidation.bPassed;
}
