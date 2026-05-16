// BlueprintHelper Review asset context utility functions.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"

class UBlueprint;
class UClass;
class UObject;

class FBlueprintHelperReviewAssetContextUtils
{
public:
	static UObject* ResolveLoadedOrLoadObject(const FString& ObjectPath, const FString& AssetPath);
	static void PopulateBlueprintContext(FBlueprintHelperReviewAssetContext& Context, UBlueprint* Blueprint);
	static void PopulateClassContext(FBlueprintHelperReviewAssetContext& Context, UClass* Class);
};
