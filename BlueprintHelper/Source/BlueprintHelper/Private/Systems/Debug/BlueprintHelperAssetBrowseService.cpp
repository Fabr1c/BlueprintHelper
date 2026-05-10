// BlueprintHelper Service Layer 。资产浏览与管理服务实。

#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperAssetBrowse, Log, All);

// ─── 辅助：FAssetData 。FBlueprintHelperAssetInfo ───

FBlueprintHelperAssetInfo FBlueprintHelperAssetBrowseService::AssetDataToInfo(const FAssetData& Data)
{
	FBlueprintHelperAssetInfo Info;
	Info.AssetPath = Data.GetObjectPathString();
	Info.AssetName = Data.AssetName.ToString();
	Info.AssetClass = Data.AssetClassPath.GetAssetName().ToString();

	// 蓝图类尝试读取父类
	FString ParentClassPath;
	if (Data.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassPath))
	{
		// 截取类名部分
		int32 DotIdx = INDEX_NONE;
		if (ParentClassPath.FindLastChar(TEXT('.'), DotIdx))
		{
			Info.ParentClass = ParentClassPath.RightChop(DotIdx + 1);
		}
		else
		{
			Info.ParentClass = ParentClassPath;
		}
	}

	// 磁盘大小（暂不可用，标记为未知）
	Info.DiskSize = -1;

	return Info;
}

// ─── ListAssets ───

FBlueprintHelperListAssetsResult FBlueprintHelperAssetBrowseService::ListAssets(
	const FBlueprintHelperListAssetsRequest& Request) const
{
	FBlueprintHelperListAssetsResult Result;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter Filter;
	if (!Request.Path.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*Request.Path));
	}
	else
	{
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
	}
	Filter.bRecursivePaths = Request.bRecursive;

	if (!Request.ClassFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(*Request.ClassFilter)));
		// 常见编辑器类型可能在其他模块
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/UMGEditor")), FName(*Request.ClassFilter)));
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/CoreUObject")), FName(*Request.ClassFilter)));
	}

	TArray<FAssetData> AssetDataList;
	Registry.GetAssets(Filter, AssetDataList);

	// 名称过滤
	Result.TotalCount = 0;
	for (const FAssetData& Data : AssetDataList)
	{
		if (!Request.NameFilter.IsEmpty())
		{
			if (!Data.AssetName.ToString().Contains(Request.NameFilter))
			{
				continue;
			}
		}
		++Result.TotalCount;
		if (Request.MaxResults > 0 && Result.Assets.Num() >= Request.MaxResults)
		{
			continue; // 继续计数但不再添加
		}
		Result.Assets.Add(AssetDataToInfo(Data));
	}

	Result.bSuccess = true;
	return Result;
}

// ─── SearchAssets ───

FBlueprintHelperListAssetsResult FBlueprintHelperAssetBrowseService::SearchAssets(
	const FBlueprintHelperListAssetsRequest& Request) const
{
	// SearchAssets 。ListAssets 共享实现，强制递归且路径默。/Game
	FBlueprintHelperListAssetsRequest SearchReq = Request;
	SearchReq.bRecursive = true;
	if (SearchReq.Path.IsEmpty())
	{
		SearchReq.Path = TEXT("/Game");
	}
	return ListAssets(SearchReq);
}

// ─── OpenAsset ───

bool FBlueprintHelperAssetBrowseService::OpenAsset(const FString& AssetPath, FString& OutError) const
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor 不可用。");
		return false;
	}

	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("资产路径为空。");
		return false;
	}

	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("无法加载资产: %s"), *AssetPath);
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSub)
	{
		OutError = TEXT("AssetEditorSubsystem 不可用。");
		return false;
	}

	const bool bOpened = AssetEditorSub->OpenEditorForAsset(Asset);
	if (!bOpened)
	{
		OutError = FString::Printf(TEXT("无法打开编辑器: %s"), *AssetPath);
		return false;
	}

	UE_LOG(LogBlueprintHelperAssetBrowse, Log, TEXT("已打开资产: %s"), *AssetPath);
	return true;
}

// ─── SaveAsset ───

FBlueprintHelperSaveResult FBlueprintHelperAssetBrowseService::SaveAsset(const FString& AssetPath) const
{
	FBlueprintHelperSaveResult Result;

	if (AssetPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("资产路径为空。");
		return Result;
	}

	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法加载资产: %s"), *AssetPath);
		return Result;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		Result.ErrorMessage = TEXT("无法获取 Package。");
		return Result;
	}

	TArray<UPackage*> Packages;
	Packages.Add(Package);

	const bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave(Packages,
		/*bCheckDirty=*/ false,
		/*bPromptToSave=*/ false) == FEditorFileUtils::EPromptReturnCode::PR_Success;

	if (bSaved)
	{
		Result.bSuccess = true;
		UE_LOG(LogBlueprintHelperAssetBrowse, Log, TEXT("已保存资产: %s"), *AssetPath);
	}
	else
	{
		Result.ErrorMessage = FString::Printf(TEXT("保存失败: %s"), *AssetPath);
	}

	return Result;
}

// ─── GetAssetInfo ───

FBlueprintHelperAssetInfo FBlueprintHelperAssetBrowseService::GetAssetInfo(
	const FString& AssetPath, bool& bOutSuccess, FString& OutError) const
{
	bOutSuccess = false;

	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("资产路径为空。");
		return {};
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
	{
		OutError = FString::Printf(TEXT("AssetRegistry 中未找到: %s"), *AssetPath);
		return {};
	}

	bOutSuccess = true;
	return AssetDataToInfo(AssetData);
}
