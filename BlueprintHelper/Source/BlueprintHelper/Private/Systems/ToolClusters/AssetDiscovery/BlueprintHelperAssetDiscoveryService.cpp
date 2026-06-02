#include "Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/PlatformProcess.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/TopLevelAssetPath.h"

namespace BlueprintHelperAssetDiscoveryServiceLocal
{
	static constexpr int32 MinLimit = 1;
	static constexpr int32 MaxLimit = 100;

	static const FTopLevelAssetPath& BlueprintClassPath()
	{
		static const FTopLevelAssetPath Path(TEXT("/Script/Engine.Blueprint"));
		return Path;
	}

	static const FTopLevelAssetPath& WidgetBlueprintClassPath()
	{
		static const FTopLevelAssetPath Path(TEXT("/Script/UMGEditor.WidgetBlueprint"));
		return Path;
	}

	static const FTopLevelAssetPath& DataTableClassPath()
	{
		static const FTopLevelAssetPath Path(TEXT("/Script/Engine.DataTable"));
		return Path;
	}

	static const FTopLevelAssetPath& DataAssetClassPath()
	{
		static const FTopLevelAssetPath Path(TEXT("/Script/Engine.DataAsset"));
		return Path;
	}

	static const FTopLevelAssetPath& UserDefinedStructClassPath()
	{
		static const FTopLevelAssetPath Path(TEXT("/Script/Engine.UserDefinedStruct"));
		return Path;
	}

	static FString NormalizePackagePath(const FString& RawPath)
	{
		FString Path = RawPath;
		Path.TrimStartAndEndInline();
		while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1);
		}
		return Path;
	}

	static bool IsUnderPackagePath(const FString& PackageName, const FString& PackagePath)
	{
		return PackageName == PackagePath || PackageName.StartsWith(PackagePath + TEXT("/"));
	}

	static bool IsEnginePath(const FString& PackagePath)
	{
		return PackagePath == TEXT("/Engine") || PackagePath.StartsWith(TEXT("/Engine/"));
	}

	static bool IsGamePath(const FString& PackagePath)
	{
		return PackagePath == TEXT("/Game") || PackagePath.StartsWith(TEXT("/Game/"));
	}

	static bool IsPluginContentPath(const FString& PackagePath)
	{
		return PackagePath.StartsWith(TEXT("/")) && !IsGamePath(PackagePath) && !IsEnginePath(PackagePath);
	}

	static bool IsPathAllowedByScopeFlags(
		const FString& PackagePath,
		const bool bIncludeEngineContent,
		const bool bIncludePluginContent)
	{
		if (IsGamePath(PackagePath))
		{
			return true;
		}
		if (IsEnginePath(PackagePath))
		{
			return bIncludeEngineContent;
		}
		if (IsPluginContentPath(PackagePath))
		{
			return bIncludePluginContent;
		}
		return false;
	}

	static bool IsAssetAllowedByScopeFlags(
		const FAssetData& AssetData,
		const bool bIncludeEngineContent,
		const bool bIncludePluginContent)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		return IsPathAllowedByScopeFlags(PackageName, bIncludeEngineContent, bIncludePluginContent);
	}

	static void AddUniquePackagePath(TArray<FName>& PackagePaths, const FString& PackagePath)
	{
		const FName PackagePathName(*PackagePath);
		if (!PackagePaths.Contains(PackagePathName))
		{
			PackagePaths.Add(PackagePathName);
		}
	}

	static TArray<FString> BuildPackagePaths(const FBlueprintHelperFindAssetsRequest& Request)
	{
		TArray<FString> PackagePaths;

		if (Request.PathPrefixes.Num() > 0)
		{
			for (const FString& RawPath : Request.PathPrefixes)
			{
				const FString Path = NormalizePackagePath(RawPath);
				if (!Path.IsEmpty() && IsPathAllowedByScopeFlags(
					Path,
					Request.bIncludeEngineContent,
					Request.bIncludePluginContent))
				{
					PackagePaths.AddUnique(Path);
				}
			}
			return PackagePaths;
		}

		PackagePaths.Add(TEXT("/Game"));
		if (Request.bIncludeEngineContent)
		{
			PackagePaths.Add(TEXT("/Engine"));
		}
		if (Request.bIncludePluginContent)
		{
			TArray<FString> RootContentPaths;
			FPackageName::QueryRootContentPaths(RootContentPaths);
			for (const FString& RawRootPath : RootContentPaths)
			{
				const FString RootPath = NormalizePackagePath(RawRootPath);
				if (IsPluginContentPath(RootPath))
				{
					PackagePaths.AddUnique(RootPath);
				}
			}
		}

		return PackagePaths;
	}

	static bool MatchesAnyPackagePath(const FAssetData& AssetData, const TArray<FString>& PackagePaths)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		for (const FString& PackagePath : PackagePaths)
		{
			if (IsUnderPackagePath(PackageName, PackagePath))
			{
				return true;
			}
		}
		return false;
	}

	static bool MatchesTextQuery(const FAssetData& AssetData, const FString& Query)
	{
		return Query.IsEmpty() ||
			AssetData.AssetName.ToString().Contains(Query, ESearchCase::IgnoreCase) ||
			AssetData.GetObjectPathString().Contains(Query, ESearchCase::IgnoreCase) ||
			AssetData.PackageName.ToString().Contains(Query, ESearchCase::IgnoreCase);
	}

	static FBlueprintHelperAssetListItem MakeAssetListItem(const FAssetData& AssetData, const FString& AssetType)
	{
		FBlueprintHelperAssetListItem Item;
		Item.AssetPath = AssetData.GetObjectPathString();
		Item.AssetType = AssetType;
		Item.AssetClass = AssetData.AssetClassPath.ToString();
		return Item;
	}

	static void AddClassPathIfMissing(TArray<FTopLevelAssetPath>& ClassPaths, const FTopLevelAssetPath& ClassPath)
	{
		if (!ClassPaths.Contains(ClassPath))
		{
			ClassPaths.Add(ClassPath);
		}
	}
}

FBlueprintHelperFindAssetsResult FBlueprintHelperAssetDiscoveryService::FindAssets(
	const FBlueprintHelperFindAssetsRequest& Request) const
{
	checkf(IsInGameThread(), TEXT("P0 AssetDiscoveryService must run on GameThread."));

	using namespace BlueprintHelperAssetDiscoveryServiceLocal;

	FBlueprintHelperFindAssetsResult Result;
	const int32 Limit = FMath::Clamp(Request.Limit, MinLimit, MaxLimit);
	Result.Data.Page.Limit = Limit;

	TArray<FString> PackagePaths = BuildPackagePaths(Request);
	if (PackagePaths.Num() == 0)
	{
		Result.bSuccess = true;
		return Result;
	}

	FARFilter Filter;
	Filter.bRecursivePaths = Request.bRecursive;
	Filter.bIncludeOnlyOnDiskAssets = true;

	for (const FString& PackagePath : PackagePaths)
	{
		AddUniquePackagePath(Filter.PackagePaths, PackagePath);
	}

	for (const FString& AssetType : Request.AssetTypes)
	{
		FTopLevelAssetPath ClassPath;
		if (!TryResolveSemanticAssetType(AssetType, ClassPath))
		{
			Result.ErrorCode = TEXT("invalid_asset_type");
			Result.ErrorMessage = FString::Printf(TEXT("Unknown asset_type: %s"), *AssetType);
			return Result;
		}
		AddClassPathIfMissing(Filter.ClassPaths, ClassPath);
	}

	for (const FString& AssetClass : Request.AssetClasses)
	{
		FTopLevelAssetPath ClassPath;
		if (!TryResolveAssetClassPath(AssetClass, ClassPath))
		{
			Result.ErrorCode = TEXT("invalid_asset_class");
			Result.ErrorMessage = FString::Printf(TEXT("Invalid asset_class full class path: %s"), *AssetClass);
			return Result;
		}
		AddClassPathIfMissing(Filter.ClassPaths, ClassPath);
	}

	FString Query = Request.Query;
	Query.TrimStartAndEndInline();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.EnumerateAssets(
		Filter,
		[&Result, &PackagePaths, &Request, &Query, Limit](const FAssetData& AssetData)
		{
			if (!MatchesAnyPackagePath(AssetData, PackagePaths) ||
				!IsAssetAllowedByScopeFlags(AssetData, Request.bIncludeEngineContent, Request.bIncludePluginContent) ||
				(!Request.bIncludeRedirectors && AssetData.IsRedirector()) ||
				!MatchesTextQuery(AssetData, Query))
			{
				return true;
			}

			Result.Data.Assets.Add(MakeAssetListItem(
				AssetData,
				FBlueprintHelperAssetDiscoveryService::ResolveSemanticAssetType(AssetData)));
			if (Result.Data.Assets.Num() > Limit)
			{
				Result.Data.Assets.SetNum(Limit);
				Result.Data.Page.bHasMore = true;
				return false;
			}

			return true;
		});

	Result.bSuccess = true;
	return Result;
}

bool FBlueprintHelperAssetDiscoveryService::TryResolveAssetClassPath(
	const FString& ClassPath,
	FTopLevelAssetPath& OutClassPath)
{
	FString NormalizedClassPath = ClassPath;
	NormalizedClassPath.TrimStartAndEndInline();
	if (!NormalizedClassPath.StartsWith(TEXT("/Script/")) || !NormalizedClassPath.Contains(TEXT(".")))
	{
		return false;
	}

	FTopLevelAssetPath ResolvedClassPath(NormalizedClassPath);
	if (!ResolvedClassPath.IsValid() || ResolvedClassPath.GetAssetName().IsNone())
	{
		return false;
	}

	OutClassPath = ResolvedClassPath;
	return true;
}

bool FBlueprintHelperAssetDiscoveryService::TryResolveSemanticAssetType(
	const FString& AssetType,
	FTopLevelAssetPath& OutClassPath)
{
	using namespace BlueprintHelperAssetDiscoveryServiceLocal;

	FString NormalizedType = AssetType;
	NormalizedType.TrimStartAndEndInline();
	NormalizedType.ToLowerInline();

	if (NormalizedType == TEXT("blueprint"))
	{
		OutClassPath = BlueprintClassPath();
		return true;
	}
	if (NormalizedType == TEXT("widget_blueprint"))
	{
		OutClassPath = WidgetBlueprintClassPath();
		return true;
	}
	if (NormalizedType == TEXT("data_table"))
	{
		OutClassPath = DataTableClassPath();
		return true;
	}
	if (NormalizedType == TEXT("data_asset"))
	{
		OutClassPath = DataAssetClassPath();
		return true;
	}
	if (NormalizedType == TEXT("user_defined_struct"))
	{
		OutClassPath = UserDefinedStructClassPath();
		return true;
	}

	return false;
}

FString FBlueprintHelperAssetDiscoveryService::ResolveSemanticAssetType(const FAssetData& AssetData)
{
	using namespace BlueprintHelperAssetDiscoveryServiceLocal;

	if (AssetData.AssetClassPath == BlueprintClassPath())
	{
		return TEXT("blueprint");
	}
	if (AssetData.AssetClassPath == WidgetBlueprintClassPath())
	{
		return TEXT("widget_blueprint");
	}
	if (AssetData.AssetClassPath == DataTableClassPath())
	{
		return TEXT("data_table");
	}
	if (AssetData.AssetClassPath == DataAssetClassPath())
	{
		return TEXT("data_asset");
	}
	if (AssetData.AssetClassPath == UserDefinedStructClassPath())
	{
		return TEXT("user_defined_struct");
	}

	return AssetData.AssetClassPath.GetAssetName().ToString();
}
