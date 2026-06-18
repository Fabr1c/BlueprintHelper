// BlueprintHelper MaterialInstance read-context projection.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceReadContextProjection
{
public:
	static bool BuildReadContextJson(
		const FString& AssetPath,
		TSharedPtr<FJsonObject>& OutJson,
		FString& OutError);
};
