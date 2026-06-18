// BlueprintHelper MaterialInstance parameter JSON helpers.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceParameterJsonUtils
{
public:
	static TSharedRef<FJsonObject> MakeParameterValueJson(
		const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter,
		const FString& AssetPath = FString());

	static TSharedRef<FJsonObject> MakeParameterSnapshotJson(
		const FString& AssetPath,
		const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter);

	static bool TryReadParameterType(
		const TSharedPtr<FJsonObject>& Json,
		EBlueprintHelperMaterialInstanceParameterType& OutType);

	static bool TryReadParameterValue(
		const TSharedPtr<FJsonObject>& Json,
		EBlueprintHelperMaterialInstanceParameterType Type,
		FBlueprintHelperMaterialInstanceParameterValue& OutValue,
		FString& OutError);

	static FString ReadParameterValueString(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FallbackField = TEXT("effective_value"));

	static FString MakeOverrideState(bool bHasOverride, const FString& Source);

	static bool SerializeJsonObject(
		const TSharedPtr<FJsonObject>& Json,
		FString& OutJsonText);

	static TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText);
};
