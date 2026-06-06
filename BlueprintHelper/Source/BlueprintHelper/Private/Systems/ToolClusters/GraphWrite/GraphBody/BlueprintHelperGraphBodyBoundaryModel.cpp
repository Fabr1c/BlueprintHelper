#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

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
	case EBlueprintHelperGraphBodyKind::K2BlockImplementation:
		return TEXT("k2.block_implementation");
	case EBlueprintHelperGraphBodyKind::K2ExternalBody:
		return TEXT("k2.external_body");
	case EBlueprintHelperGraphBodyKind::ReservedMacroBody:
		return TEXT("k2.macro_body");
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
	if (Normalized == TEXT("k2.block_implementation"))
	{
		return EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	}
	if (Normalized == TEXT("k2.external_body"))
	{
		return EBlueprintHelperGraphBodyKind::K2ExternalBody;
	}
	if (Normalized == TEXT("k2.macro_body"))
	{
		return EBlueprintHelperGraphBodyKind::ReservedMacroBody;
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
