#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

static void BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(
	TSharedRef<FJsonObject> Json,
	const TCHAR* FieldName,
	const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> ArrayValues;
	for (const FString& Value : Values)
	{
		if (!Value.IsEmpty())
		{
			ArrayValues.Add(MakeShared<FJsonValueString>(Value));
		}
	}
	Json->SetArrayField(FieldName, ArrayValues);
}

FString FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(EBlueprintHelperGraphBodyKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphBodyKind::K2CustomEventBody:
		return TEXT("k2.custom_event_body");
	case EBlueprintHelperGraphBodyKind::K2EventBody:
		return TEXT("k2.event_body");
	case EBlueprintHelperGraphBodyKind::K2FunctionBody:
		return TEXT("k2.function_body");
	case EBlueprintHelperGraphBodyKind::K2MacroBody:
		return TEXT("k2.macro_body");
	case EBlueprintHelperGraphBodyKind::K2BlockImplementation:
		return TEXT("k2.block_implementation");
	case EBlueprintHelperGraphBodyKind::K2ExternalBody:
		return TEXT("k2.external_body");
	case EBlueprintHelperGraphBodyKind::ReservedMaterialFunctionBody:
		return TEXT("material.function_body");
	case EBlueprintHelperGraphBodyKind::ReservedAnimationGraphBody:
		return TEXT("animation.graph_body");
	default:
		return TEXT("unknown");
	}
}

EBlueprintHelperGraphBodyKind FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindFromString(const FString& Value)
{
	const FString Normalized = Value.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("k2.custom_event_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2CustomEventBody;
	}
	if (Normalized == TEXT("k2.event_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2EventBody;
	}
	if (Normalized == TEXT("k2.function_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2FunctionBody;
	}
	if (Normalized == TEXT("k2.macro_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2MacroBody;
	}
	if (Normalized == TEXT("k2.block_implementation"))
	{
		return EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	}
	if (Normalized == TEXT("k2.external_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2ExternalBody;
	}
	if (Normalized == TEXT("material.function_body"))
	{
		return EBlueprintHelperGraphBodyKind::ReservedMaterialFunctionBody;
	}
	if (Normalized == TEXT("animation.graph_body"))
	{
		return EBlueprintHelperGraphBodyKind::ReservedAnimationGraphBody;
	}
	return EBlueprintHelperGraphBodyKind::Unknown;
}

FString FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(
	const FBlueprintHelperGraphBodyBoundaryModel& Model)
{
	return FString::Printf(
		TEXT("%s|%s|%s|%s"),
		*Model.TargetAssetPath,
		*Model.GraphName,
		*BodyKindToString(Model.BodyKind),
		*Model.OwnedBlockId);
}

TSharedRef<FJsonObject> FBlueprintHelperGraphBodyBoundaryModelUtils::ToJsonObject(
	const FBlueprintHelperGraphBodyBoundaryModel& Model)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.GraphBodyBoundaryEvidence.v1"));
	if (!Model.RuntimeAdapterId.IsEmpty())
	{
		Json->SetStringField(TEXT("runtime_adapter_id"), Model.RuntimeAdapterId);
	}
	if (!Model.TaskSpecStrategy.IsEmpty())
	{
		Json->SetStringField(TEXT("task_spec_strategy"), Model.TaskSpecStrategy);
	}
	if (!Model.TargetAssetPath.IsEmpty())
	{
		Json->SetStringField(TEXT("asset_path"), Model.TargetAssetPath);
	}
	if (!Model.GraphName.IsEmpty())
	{
		Json->SetStringField(TEXT("graph_name"), Model.GraphName);
	}
	if (!Model.GraphFamily.IsEmpty())
	{
		Json->SetStringField(TEXT("graph_family"), Model.GraphFamily);
	}
	if (!Model.OwnedBlockId.IsEmpty())
	{
		Json->SetStringField(TEXT("owned_block_id"), Model.OwnedBlockId);
	}
	Json->SetStringField(TEXT("body_kind"), BodyKindToString(Model.BodyKind));
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("entry_boundaries"), Model.EntryNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("exit_boundaries"), Model.ExitNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("protected_node_refs"), Model.ProtectedNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("deletable_node_refs"), Model.DeletableNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("generated_node_refs"), Model.GeneratedNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("imported_body_node_refs"), Model.ImportedBodyNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("reachable_body_flow_node_refs"), Model.ReachableBodyFlowNodeRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("external_anchor_refs"), Model.ExternalAnchorRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("semantic_source_refs"), Model.SemanticSourceRefs);
	BlueprintHelperGraphBodyBoundaryModelSetStringArrayField(Json, TEXT("connectivity_exception_codes"), Model.ConnectivityExceptionCodes);
	return Json;
}
