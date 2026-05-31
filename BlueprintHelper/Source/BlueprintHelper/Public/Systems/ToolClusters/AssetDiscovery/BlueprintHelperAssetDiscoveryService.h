#pragma once

#include "Shared/AssetDiscovery/BlueprintHelperAssetDiscoveryTypes.h"

struct FAssetData;
struct FTopLevelAssetPath;

struct BLUEPRINTHELPER_API FBlueprintHelperFindAssetsResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString ErrorMessage;
	FBlueprintHelperFindAssetsResultData Data;
};

class BLUEPRINTHELPER_API FBlueprintHelperAssetDiscoveryService
{
public:
	FBlueprintHelperFindAssetsResult FindAssets(const FBlueprintHelperFindAssetsRequest& Request) const;

private:
	static bool TryResolveAssetClassPath(const FString& ClassPath, FTopLevelAssetPath& OutClassPath);
	static bool TryResolveSemanticAssetType(const FString& AssetType, FTopLevelAssetPath& OutClassPath);
	static FString ResolveSemanticAssetType(const FAssetData& AssetData);
};
