#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperExternalGraphAnchorRole : uint8
{
	Node,
	ExecBoundary,
	BodyEntry
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchor
{
	static constexpr const TCHAR* SchemaString = TEXT("BlueprintHelper.ExternalGraphAnchor.v1");

	FString Schema = SchemaString;
	FString AssetPath;
	FString GraphName;
	FString NodeGuid;
	FString NodeClass;
	FString StableName;
	FString EntryKind;
	FString MemberName;
	FString FunctionName;
	FString DisplayName;
	FString PinName;
	FString PinDirection;
	EBlueprintHelperExternalGraphAnchorRole SemanticRole = EBlueprintHelperExternalGraphAnchorRole::Node;
	FString Fingerprint;

	TSharedRef<FJsonObject> ToJson() const;

	static FString RoleToString(EBlueprintHelperExternalGraphAnchorRole Role);
	static bool TryRoleFromString(const FString& Value, EBlueprintHelperExternalGraphAnchorRole& OutRole);
	static bool FromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperExternalGraphAnchor& OutAnchor, FString& OutError);
};

struct BLUEPRINTHELPER_API FBlueprintHelperLogicJsonAnchorSelector
{
	static constexpr const TCHAR* SchemaString = TEXT("BlueprintHelper.LogicJsonAnchorSelector.v1");

	FString Schema = SchemaString;
	FString AssetPath;
	FString GraphName;
	FString EntryName;
	FString NodeRef;
	FString LinkRef;
	FString PinRef;

	static bool FromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperLogicJsonAnchorSelector& OutSelector,
		FString& OutError);
};

enum class EBlueprintHelperExternalCompactAnchorType : uint8
{
	Node,
	Pin,
	Link,
	Body,
	Unknown
};

enum class EBlueprintHelperExternalCompactLinkKind : uint8
{
	Exec,
	Data,
	Unknown
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalCompactAnchor
{
	FString AnchorType;
	FString AnchorRef;

	EBlueprintHelperExternalCompactAnchorType Type = EBlueprintHelperExternalCompactAnchorType::Unknown;
	EBlueprintHelperExternalCompactLinkKind LinkKind = EBlueprintHelperExternalCompactLinkKind::Unknown;
	FString NodeKey;
	FString PinKey;
	FString SourceNodeKey;
	FString SourcePinKey;
	FString TargetNodeKey;
	FString TargetPinKey;
	FString Fingerprint;

	static bool FromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperExternalCompactAnchor& OutAnchor,
		FString& OutError);
};
