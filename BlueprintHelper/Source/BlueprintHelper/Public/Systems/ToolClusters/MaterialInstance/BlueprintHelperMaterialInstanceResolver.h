// BlueprintHelper MaterialInstance resolver.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceResolver
{
public:
	static FString NormalizeMaterialInstanceObjectPath(const FString& AssetPath);

	static FBlueprintHelperMaterialInstanceAssetResolveResult ResolveAsset(const FString& AssetPath);

	static bool CollectParameterSchema(
		UMaterialInstanceConstant* Instance,
		TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry>& OutSchema,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static FBlueprintHelperMaterialInstanceParameterResolveResult ResolveParameter(
		UMaterialInstanceConstant* Instance,
		const FName ParameterName,
		EBlueprintHelperMaterialInstanceParameterType RequestedType =
			EBlueprintHelperMaterialInstanceParameterType::Unknown);

	static bool TryParseParameterType(
		const FString& Input,
		EBlueprintHelperMaterialInstanceParameterType& OutType);
};
