#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

TSharedRef<FJsonObject> FBlueprintHelperExternalGraphAnchor::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Schema.IsEmpty() ? SchemaString : Schema);
	Json->SetStringField(TEXT("asset_path"), AssetPath);
	Json->SetStringField(TEXT("graph_name"), GraphName);
	Json->SetStringField(TEXT("node_guid"), NodeGuid);
	Json->SetStringField(TEXT("node_class"), NodeClass);
	if (!PinName.IsEmpty())
	{
		Json->SetStringField(TEXT("pin_name"), PinName);
	}
	if (!PinDirection.IsEmpty())
	{
		Json->SetStringField(TEXT("pin_direction"), PinDirection);
	}
	Json->SetStringField(TEXT("semantic_role"), RoleToString(SemanticRole));
	Json->SetStringField(TEXT("fingerprint"), Fingerprint);
	return Json;
}

FString FBlueprintHelperExternalGraphAnchor::RoleToString(EBlueprintHelperExternalGraphAnchorRole Role)
{
	switch (Role)
	{
	case EBlueprintHelperExternalGraphAnchorRole::Node:
		return TEXT("node");
	case EBlueprintHelperExternalGraphAnchorRole::ExecBoundary:
		return TEXT("exec_boundary");
	case EBlueprintHelperExternalGraphAnchorRole::BodyEntry:
		return TEXT("body_entry");
	default:
		return TEXT("node");
	}
}

bool FBlueprintHelperExternalGraphAnchor::TryRoleFromString(
	const FString& Value,
	EBlueprintHelperExternalGraphAnchorRole& OutRole)
{
	if (Value.Equals(TEXT("node"), ESearchCase::IgnoreCase))
	{
		OutRole = EBlueprintHelperExternalGraphAnchorRole::Node;
		return true;
	}
	if (Value.Equals(TEXT("exec_boundary"), ESearchCase::IgnoreCase))
	{
		OutRole = EBlueprintHelperExternalGraphAnchorRole::ExecBoundary;
		return true;
	}
	if (Value.Equals(TEXT("body_entry"), ESearchCase::IgnoreCase))
	{
		OutRole = EBlueprintHelperExternalGraphAnchorRole::BodyEntry;
		return true;
	}
	return false;
}

bool FBlueprintHelperExternalGraphAnchor::FromJson(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperExternalGraphAnchor& OutAnchor,
	FString& OutError)
{
	if (!Json.IsValid())
	{
		OutError = TEXT("external_anchor_schema_unsupported");
		return false;
	}

	FString SchemaValue;
	if (!Json->TryGetStringField(TEXT("schema"), SchemaValue) || SchemaValue != SchemaString)
	{
		OutError = TEXT("external_anchor_schema_unsupported");
		return false;
	}

	FString RoleValue;
	if (!Json->TryGetStringField(TEXT("semantic_role"), RoleValue)
		|| !TryRoleFromString(RoleValue, OutAnchor.SemanticRole))
	{
		OutError = TEXT("external_anchor_schema_unsupported");
		return false;
	}

	OutAnchor.Schema = SchemaValue;
	Json->TryGetStringField(TEXT("asset_path"), OutAnchor.AssetPath);
	Json->TryGetStringField(TEXT("graph_name"), OutAnchor.GraphName);
	Json->TryGetStringField(TEXT("node_guid"), OutAnchor.NodeGuid);
	Json->TryGetStringField(TEXT("node_class"), OutAnchor.NodeClass);
	Json->TryGetStringField(TEXT("pin_name"), OutAnchor.PinName);
	Json->TryGetStringField(TEXT("pin_direction"), OutAnchor.PinDirection);
	Json->TryGetStringField(TEXT("fingerprint"), OutAnchor.Fingerprint);
	return true;
}
