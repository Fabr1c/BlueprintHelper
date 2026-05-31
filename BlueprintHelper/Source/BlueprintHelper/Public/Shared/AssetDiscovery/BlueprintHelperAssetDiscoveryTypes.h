// BlueprintHelper Service Layer - Asset Discovery DTO definitions

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FBlueprintHelperFindAssetsRequest
{
	FString Schema = TEXT("BlueprintHelper.FindAssetsRequest.v1");
	FString Query;
	TArray<FString> PathPrefixes;
	TArray<FString> AssetTypes;
	TArray<FString> AssetClasses;
	bool bRecursive = true;
	int32 Limit = 20;
	bool bIncludePluginContent = false;
	bool bIncludeEngineContent = false;
	bool bIncludeRedirectors = false;
};

struct FBlueprintHelperAssetListItem
{
	FString AssetPath;
	FString AssetType;
	FString AssetClass;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("asset_type"), AssetType);
		Json->SetStringField(TEXT("asset_class"), AssetClass);
		return Json;
	}
};

struct FBlueprintHelperAssetPageInfo
{
	int32 Limit = 20;
	bool bHasMore = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("limit"), Limit);
		Json->SetBoolField(TEXT("has_more"), bHasMore);
		return Json;
	}
};

struct FBlueprintHelperFindAssetsResultData
{
	FString Schema = TEXT("FindAssets.v1");
	TArray<FBlueprintHelperAssetListItem> Assets;
	FBlueprintHelperAssetPageInfo Page;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);

		TArray<TSharedPtr<FJsonValue>> AssetValues;
		for (const FBlueprintHelperAssetListItem& Asset : Assets)
		{
			AssetValues.Add(MakeShared<FJsonValueObject>(Asset.ToJson()));
		}

		Json->SetArrayField(TEXT("assets"), AssetValues);
		Json->SetObjectField(TEXT("page"), Page.ToJson());
		return Json;
	}
};
