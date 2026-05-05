// BlueprintHelper Service Layer — 通用 UObject 属性反射服务

#pragma once

#include "CoreMinimal.h"

class UObject;

// ─── 属性信息 ───

/** 单个 UObject 属性的摘要信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyInfo
{
	/** 属性名称。 */
	FString Name;

	/** 属性类型（如 FloatProperty、StructProperty、ArrayProperty）。 */
	FString TypeName;

	/** 当前值的文本表示。 */
	FString Value;

	/** 属性类别（Category 元数据）。 */
	FString Category;

	/** CPF 标志摘要（EditAnywhere、BlueprintReadWrite 等）。 */
	FString Flags;
};

// ─── 查询结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertiesResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	/** 对象类名。 */
	FString ClassName;
	/** 资产路径。 */
	FString AssetPath;
	TArray<FBlueprintHelperObjectPropertyInfo> Properties;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetPropertyResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString PropertyName;
	FString OldValue;
	FString NewValue;
};

/**
 * 通用 UObject 属性反射服务。
 * 通过 FProperty 遍历读取/写入任意 UObject（包括 DataAsset）的属性。
 */
class BLUEPRINTHELPER_API FBlueprintHelperPropertyReflectionService
{
public:
	/** 获取资产的所有可编辑属性。 */
	FBlueprintHelperObjectPropertiesResult GetObjectProperties(const FString& AssetPath) const;

	/** 设置资产的单个属性值。 */
	FBlueprintHelperSetPropertyResult SetObjectProperty(
		const FString& AssetPath,
		const FString& PropertyName,
		const FString& Value) const;

private:
	/** 根据资产路径加载 UObject。 */
	UObject* ResolveAsset(const FString& AssetPath, FString& OutError) const;

	/** 构建属性标志摘要字符串。 */
	static FString BuildFlagsSummary(uint64 PropertyFlags);
};
