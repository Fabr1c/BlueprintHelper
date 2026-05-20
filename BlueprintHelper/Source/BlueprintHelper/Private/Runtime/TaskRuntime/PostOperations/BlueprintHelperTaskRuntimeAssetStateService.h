#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperTaskRuntimeAssetState
{
	FString AssetPath;
	FString PackageName;
	FString ObjectPath;
	bool bPackageLoaded = false;
	bool bPackageDirty = false;
	bool bAssetLoaded = false;
	bool bIsBlueprint = false;
};

class FBlueprintHelperTaskRuntimeAssetStateService
{
public:
	static FString NormalizePackageName(const FString& AssetPath);
	static FString BuildObjectPath(const FString& AssetPath);
	static FBlueprintHelperTaskRuntimeAssetState ReadState(const FString& AssetPath);
};
