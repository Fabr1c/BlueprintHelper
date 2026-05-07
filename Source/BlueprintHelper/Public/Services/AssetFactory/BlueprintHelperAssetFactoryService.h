// BlueprintHelper Service Layer 。Asset Factory 服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/AssetFactory/BlueprintHelperAssetFactoryTypes.h"

struct FBlueprintHelperToolResultBase;
class FBlueprintHelperCompileService;

/**
 * 资产创建服务。
 * 负责校验、冲突处理、创。UE 资产并返回标。ToolResultBase。
 * 第一版支持：blueprint_class, blueprint_interface, structure, input_action, input_mapping_context。
 * 不负责：添加组件、修。Class Settings、写图表、创。Block。
 */
class BLUEPRINTHELPER_API FBlueprintHelperAssetFactoryService
{
public:
	FBlueprintHelperAssetFactoryService();

	/**
	 * 创建资产。
	 * @param AssetPath 目标路径。Game/...。
	 * @param AssetType 资产类型
	 * @param ParentClass 父类（Blueprint 需要）
	 * @param ValueType Input Action 值类型
	 * @param CollisionPolicy 冲突策略
	 */
	FBlueprintHelperAssetFactoryData CreateAsset(
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType,
		const FString& ParentClass = TEXT(""),
		const FString& ValueType = TEXT(""),
		EBlueprintHelperAssetCollisionPolicy CollisionPolicy = EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		bool bDryRun = false) const;

	/** 根据 asset_type 确定是否需要编译。*/
	static bool ShouldCompile(EBlueprintHelperAssetType AssetType);

	/** 根据 asset_type 确定是否需要保存。*/
	static bool ShouldSave(EBlueprintHelperAssetType AssetType);

	/** Parses TaskSpec/Bridge asset_type aliases and fills BlueprintClass parent defaults. */
	static bool TryNormalizeAssetTypeAndParent(
		const FString& AssetTypeText,
		FString& InOutParentClass,
		EBlueprintHelperAssetType& OutAssetType);

private:
	/** 解析 asset_type 。factory_type。*/
	static EBlueprintHelperFactoryType AssetTypeToFactoryType(EBlueprintHelperAssetType Type);

	/** 解析 asset_type 。UE 资产类字符串。*/
	static FString AssetTypeToAssetClass(EBlueprintHelperAssetType Type);

	/** 检查资产是否已存在。*/
	static bool AssetExists(const FString& AssetPath);

	/** 获取已存在资产的类。*/
	static FString GetExistingAssetClass(const FString& AssetPath);

	/** 实际创建蓝图类。*/
	static bool CreateBlueprintClass(const FString& AssetPath, const FString& ParentClass);

	/** 实际创建蓝图接口。*/
	static bool CreateBlueprintInterface(const FString& AssetPath);

	/** 实际创建结构体。*/
	static bool CreateStructure(const FString& AssetPath);

	/** 实际创建 Input Action。*/
	static bool CreateInputAction(const FString& AssetPath, const FString& ValueType);

	/** 实际创建 Input Mapping Context。*/
	static bool CreateInputMappingContext(const FString& AssetPath);

	/** 实际创建 DataAsset。*/
	static bool CreateDataAsset(const FString& AssetPath, const FString& AssetClass);
};
