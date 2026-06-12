#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

struct FBlueprintHelperCompactAnchorParts
{
	FString Prefix;
	FString Body;
	FString Fingerprint;
};

static bool BlueprintHelperSplitCompactAnchorRef(
	const FString& AnchorRef,
	const FString& ExpectedPrefix,
	FBlueprintHelperCompactAnchorParts& OutParts)
{
	if (!AnchorRef.StartsWith(ExpectedPrefix))
	{
		return false;
	}

	FString Left;
	if (!AnchorRef.Split(TEXT("#"), &Left, &OutParts.Fingerprint, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
		OutParts.Fingerprint.IsEmpty())
	{
		return false;
	}

	OutParts.Prefix = ExpectedPrefix;
	OutParts.Body = Left.RightChop(ExpectedPrefix.Len());
	return !OutParts.Body.IsEmpty();
}

static EBlueprintHelperExternalCompactLinkKind BlueprintHelperCompactLinkKindFromPrefix(const FString& Value)
{
	if (Value.Equals(TEXT("e"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperExternalCompactLinkKind::Exec;
	}
	if (Value.Equals(TEXT("d"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperExternalCompactLinkKind::Data;
	}
	return EBlueprintHelperExternalCompactLinkKind::Unknown;
}

static bool BlueprintHelperParseCompactPinBody(
	const FString& Body,
	EBlueprintHelperExternalCompactLinkKind& OutKind,
	FString& OutNodeKey,
	FString& OutPinKey)
{
	FString KindPart;
	FString EndpointPart;
	if (!Body.Split(TEXT(":"), &KindPart, &EndpointPart) || EndpointPart.IsEmpty())
	{
		return false;
	}

	FString NodePart;
	FString PinPart;
	if (!EndpointPart.Split(TEXT("."), &NodePart, &PinPart) || NodePart.IsEmpty() || PinPart.IsEmpty())
	{
		return false;
	}

	OutKind = BlueprintHelperCompactLinkKindFromPrefix(KindPart);
	OutNodeKey = NodePart;
	OutPinKey = PinPart;
	return OutKind != EBlueprintHelperExternalCompactLinkKind::Unknown;
}

static bool BlueprintHelperParseCompactLinkBody(
	const FString& Body,
	EBlueprintHelperExternalCompactLinkKind& OutKind,
	FString& OutSourceNodeKey,
	FString& OutSourcePinKey,
	FString& OutTargetNodeKey,
	FString& OutTargetPinKey)
{
	FString KindPart;
	FString EndpointsPart;
	if (!Body.Split(TEXT(":"), &KindPart, &EndpointsPart) || EndpointsPart.IsEmpty())
	{
		return false;
	}

	FString SourcePart;
	FString TargetPart;
	if (!EndpointsPart.Split(TEXT(">"), &SourcePart, &TargetPart) || SourcePart.IsEmpty() || TargetPart.IsEmpty())
	{
		return false;
	}

	if (!SourcePart.Split(TEXT("."), &OutSourceNodeKey, &OutSourcePinKey) ||
		!TargetPart.Split(TEXT("."), &OutTargetNodeKey, &OutTargetPinKey) ||
		OutSourceNodeKey.IsEmpty() ||
		OutSourcePinKey.IsEmpty() ||
		OutTargetNodeKey.IsEmpty() ||
		OutTargetPinKey.IsEmpty())
	{
		return false;
	}

	OutKind = BlueprintHelperCompactLinkKindFromPrefix(KindPart);
	return OutKind != EBlueprintHelperExternalCompactLinkKind::Unknown;
}

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

bool FBlueprintHelperExternalCompactAnchor::FromJson(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperExternalCompactAnchor& OutAnchor,
	FString& OutError)
{
	if (!Json.IsValid())
	{
		OutError = TEXT("external_anchor_ref_invalid");
		return false;
	}

	OutAnchor = FBlueprintHelperExternalCompactAnchor();
	if (!Json->TryGetStringField(TEXT("anchor_type"), OutAnchor.AnchorType) ||
		!Json->TryGetStringField(TEXT("anchor_ref"), OutAnchor.AnchorRef) ||
		OutAnchor.AnchorType.IsEmpty() ||
		OutAnchor.AnchorRef.IsEmpty())
	{
		OutError = TEXT("external_anchor_ref_invalid");
		return false;
	}

	FBlueprintHelperCompactAnchorParts Parts;
	if (OutAnchor.AnchorType.Equals(TEXT("external_pin"), ESearchCase::IgnoreCase))
	{
		if (!BlueprintHelperSplitCompactAnchorRef(OutAnchor.AnchorRef, TEXT("xpin:v1:"), Parts) ||
			!BlueprintHelperParseCompactPinBody(Parts.Body, OutAnchor.LinkKind, OutAnchor.NodeKey, OutAnchor.PinKey))
		{
			OutError = TEXT("external_anchor_ref_invalid");
			return false;
		}
		OutAnchor.Type = EBlueprintHelperExternalCompactAnchorType::Pin;
		OutAnchor.Fingerprint = Parts.Fingerprint;
		return true;
	}

	if (OutAnchor.AnchorType.Equals(TEXT("external_node"), ESearchCase::IgnoreCase))
	{
		if (!BlueprintHelperSplitCompactAnchorRef(OutAnchor.AnchorRef, TEXT("xnode:v1:"), Parts))
		{
			OutError = TEXT("external_anchor_ref_invalid");
			return false;
		}
		OutAnchor.Type = EBlueprintHelperExternalCompactAnchorType::Node;
		OutAnchor.NodeKey = Parts.Body;
		OutAnchor.Fingerprint = Parts.Fingerprint;
		return true;
	}

	if (OutAnchor.AnchorType.Equals(TEXT("external_link"), ESearchCase::IgnoreCase))
	{
		if (!BlueprintHelperSplitCompactAnchorRef(OutAnchor.AnchorRef, TEXT("xlink:v1:"), Parts) ||
			!BlueprintHelperParseCompactLinkBody(
				Parts.Body,
				OutAnchor.LinkKind,
				OutAnchor.SourceNodeKey,
				OutAnchor.SourcePinKey,
				OutAnchor.TargetNodeKey,
				OutAnchor.TargetPinKey))
		{
			OutError = TEXT("external_anchor_ref_invalid");
			return false;
		}
		OutAnchor.Type = EBlueprintHelperExternalCompactAnchorType::Link;
		OutAnchor.Fingerprint = Parts.Fingerprint;
		return true;
	}

	OutError = TEXT("external_anchor_ref_unsupported");
	return false;
}

bool FBlueprintHelperLogicJsonAnchorSelector::FromJson(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperLogicJsonAnchorSelector& OutSelector,
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

	OutSelector = FBlueprintHelperLogicJsonAnchorSelector();
	OutSelector.Schema = SchemaValue;
	Json->TryGetStringField(TEXT("asset_path"), OutSelector.AssetPath);
	FString GraphNameValue;
	FString GraphAliasValue;
	const bool bHasGraphName = Json->TryGetStringField(TEXT("graph_name"), GraphNameValue);
	const bool bHasGraphAlias = Json->TryGetStringField(TEXT("graph"), GraphAliasValue);
	if (bHasGraphName && bHasGraphAlias && !GraphNameValue.Equals(GraphAliasValue, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_selector_invalid");
		return false;
	}
	OutSelector.GraphName = bHasGraphName ? GraphNameValue : GraphAliasValue;
	Json->TryGetStringField(TEXT("entry_name"), OutSelector.EntryName);
	Json->TryGetStringField(TEXT("node_ref"), OutSelector.NodeRef);
	Json->TryGetStringField(TEXT("link_ref"), OutSelector.LinkRef);
	Json->TryGetStringField(TEXT("pin_ref"), OutSelector.PinRef);

	if (OutSelector.AssetPath.IsEmpty() || OutSelector.GraphName.IsEmpty())
	{
		OutError = TEXT("external_anchor_selector_invalid");
		return false;
	}

	const bool bHasNodeRef = !OutSelector.NodeRef.IsEmpty();
	const bool bHasLinkRef = !OutSelector.LinkRef.IsEmpty();
	if (bHasNodeRef == bHasLinkRef)
	{
		OutError = TEXT("external_anchor_selector_invalid");
		return false;
	}
	if (bHasNodeRef && OutSelector.PinRef.IsEmpty())
	{
		OutError = TEXT("target_pin_not_found");
		return false;
	}

	return true;
}
