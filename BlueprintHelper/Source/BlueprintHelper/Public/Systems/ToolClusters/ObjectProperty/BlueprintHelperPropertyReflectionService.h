// BlueprintHelper Service Layer — 通用 UObject 属性反射服务

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "UObject/UnrealType.h"

class UObject;
class FJsonValue;

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
	bool bDryRun = false;
	FString ErrorMessage;
	FString PropertyName;
	FString PropertyPath;
	FString OldValue;
	FString NewValue;
};

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyTextSetting
{
	FString PropertyPath;
	FString Value;
};

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertySetting
{
	FString PropertyPath;
	TSharedPtr<FJsonValue> Value;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetObjectPropertiesRequest
{
	FString AssetPath;
	TArray<FBlueprintHelperObjectPropertySetting> Settings;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperInvalidObjectPropertySetting
{
	FString PropertyPath;
	FString Code;
	FString ExpectedType;
	FString ActualType;
	FString ValueSummary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("property_path"), PropertyPath);
		Json->SetStringField(TEXT("code"), Code);
		if (!ExpectedType.IsEmpty()) Json->SetStringField(TEXT("expected_type"), ExpectedType);
		if (!ActualType.IsEmpty()) Json->SetStringField(TEXT("actual_type"), ActualType);
		if (!ValueSummary.IsEmpty()) Json->SetStringField(TEXT("value_summary"), ValueSummary.Left(128));
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyWriteResult
{
	FString Mode;
	int32 RequestedCount = 0;
	int32 AppliedCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
	TArray<FBlueprintHelperInvalidObjectPropertySetting> InvalidSettings;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("mode"), Mode);
		Json->SetNumberField(TEXT("requested_count"), RequestedCount);
		Json->SetNumberField(TEXT("applied_count"), AppliedCount);
		Json->SetNumberField(TEXT("changed_count"), ChangedCount);
		Json->SetNumberField(TEXT("no_op_count"), NoOpCount);

		TArray<TSharedPtr<FJsonValue>> Invalid;
		for (const FBlueprintHelperInvalidObjectPropertySetting& Setting : InvalidSettings)
		{
			Invalid.Add(MakeShared<FJsonValueObject>(Setting.ToJson()));
		}
		Json->SetArrayField(TEXT("invalid_settings"), Invalid);
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperSetPropertiesResult
{
	bool bSuccess = false;
	bool bDryRun = false;
	FString ErrorMessage;
	FString AssetPath;
	FBlueprintHelperObjectPropertyWriteResult PropertyResult;
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
		const FString& Value,
		bool bDryRun = false) const;

	/** 设置资产的多个属性值。 */
	FBlueprintHelperSetPropertiesResult SetObjectProperties(
		const FString& AssetPath,
		const TArray<FBlueprintHelperObjectPropertyTextSetting>& Settings,
		bool bDryRun = false) const;

	/** TaskRuntime facade: 设置单个属性并返回公共 ToolResultBase。 */
	FBlueprintHelperToolResultBase SetObjectProperty(
		const FBlueprintHelperSetObjectPropertiesRequest& Request) const;

	/** TaskRuntime facade: 设置多个属性并返回公共 ToolResultBase。 */
	FBlueprintHelperToolResultBase SetObjectProperties(
		const FBlueprintHelperSetObjectPropertiesRequest& Request) const;

private:
	/** 根据资产路径加载 UObject。 */
	UObject* ResolveAsset(const FString& AssetPath, FString& OutError) const;

	/** 构建属性标志摘要字符串。 */
	static FString BuildFlagsSummary(uint64 PropertyFlags);

	static bool ResolvePropertyPath(
		UObject* RootObject,
		const FString& PropertyPath,
		FProperty*& OutProperty,
		void*& OutValuePtr,
		FString& OutExpectedType,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static bool JsonValueToImportText(
		const TSharedPtr<FJsonValue>& Value,
		FString& OutText,
		FString& OutSummary,
		FString& OutActualType,
		FString& OutError);
};
