// BlueprintHelper Service Layer — 资产浏览与管理服务

#pragma once

#include "CoreMinimal.h"

struct FAssetData;

// ─── 资产摘要 ───

/** 单条资产信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperAssetInfo
{
	/** 资产完整路径。 */
	FString AssetPath;

	/** 资产名称（不含路径）。 */
	FString AssetName;

	/** 资产类型（例如 Blueprint、WidgetBlueprint、DataTable）。 */
	FString AssetClass;

	/** 父类名称（蓝图类适用）。 */
	FString ParentClass;

	/** 磁盘占用字节数（-1 表示未知）。 */
	int64 DiskSize = -1;
};

// ─── 列表请求 ───

/** list_assets / search_assets 请求参数。 */
struct BLUEPRINTHELPER_API FBlueprintHelperListAssetsRequest
{
	/** 搜索目录（Content 相对路径，如 /Game/Blueprints）。 */
	FString Path;

	/** 可选类型过滤（如 Blueprint、DataTable）。 */
	FString ClassFilter;

	/** 可选名称关键词过滤（子串匹配）。 */
	FString NameFilter;

	/** 是否递归搜索子目录。 */
	bool bRecursive = true;

	/** 最大返回数量（0 = 不限）。 */
	int32 MaxResults = 200;
};

// ─── 列表结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperListAssetsResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FBlueprintHelperAssetInfo> Assets;
	int32 TotalCount = 0;
};

// ─── 保存结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperSaveResult
{
	bool bSuccess = false;
	FString ErrorMessage;
};

/**
 * 资产浏览与管理服务。
 * 提供资产搜索、打开、保存、信息查询等编辑器操作的无头封装。
 */
class BLUEPRINTHELPER_API FBlueprintHelperAssetBrowseService
{
public:
	/** 列出指定目录下的资产。 */
	FBlueprintHelperListAssetsResult ListAssets(const FBlueprintHelperListAssetsRequest& Request) const;

	/** 按关键词搜索资产（名称子串 + 可选类型）。 */
	FBlueprintHelperListAssetsResult SearchAssets(const FBlueprintHelperListAssetsRequest& Request) const;

	/** 打开指定资产的编辑器。 */
	bool OpenAsset(const FString& AssetPath, FString& OutError) const;

	/** 保存指定资产。 */
	FBlueprintHelperSaveResult SaveAsset(const FString& AssetPath) const;

	/** 获取资产详细信息。 */
	FBlueprintHelperAssetInfo GetAssetInfo(const FString& AssetPath, bool& bOutSuccess, FString& OutError) const;

private:
	/** 从 FAssetData 填充 AssetInfo。 */
	static FBlueprintHelperAssetInfo AssetDataToInfo(const FAssetData& Data);
};
