// BlueprintHelper Service Layer — 资产浏览与管理服务

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

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

// ─── 保存结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperSaveResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString ErrorMessage;
	TSharedPtr<FJsonObject> SourceControlJson;
};

/**
 * 资产浏览与管理服务。
 * 提供资产搜索、打开、保存、信息查询等编辑器操作的无头封装。
 */
class BLUEPRINTHELPER_API FBlueprintHelperAssetBrowseService
{
public:
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
