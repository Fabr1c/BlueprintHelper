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
	FString PinName;
	FString PinDirection;
	EBlueprintHelperExternalGraphAnchorRole SemanticRole = EBlueprintHelperExternalGraphAnchorRole::Node;
	FString Fingerprint;

	TSharedRef<FJsonObject> ToJson() const;

	static FString RoleToString(EBlueprintHelperExternalGraphAnchorRole Role);
	static bool TryRoleFromString(const FString& Value, EBlueprintHelperExternalGraphAnchorRole& OutRole);
	static bool FromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperExternalGraphAnchor& OutAnchor, FString& OutError);
};
