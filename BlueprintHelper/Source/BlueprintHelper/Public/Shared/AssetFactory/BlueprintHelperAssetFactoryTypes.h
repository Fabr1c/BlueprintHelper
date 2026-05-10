// BlueprintHelper Service Layer — Asset Factory 类型定义
// 第 4 簇：资产创建工具的数据类型

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ─── 资产类型枚举 ───

/** BlueprintHelper 语义资产类型。 */
enum class EBlueprintHelperAssetType : uint8
{
	BlueprintClass,
	BlueprintInterface,
	Structure,
	InputAction,
	InputMappingContext,
	DataAsset,
	DataTable,
	WidgetBlueprint,
	Material,
	Unknown
};

inline const TCHAR* AssetTypeToString(EBlueprintHelperAssetType Type)
{
	switch (Type)
	{
	case EBlueprintHelperAssetType::BlueprintClass:        return TEXT("blueprint_class");
	case EBlueprintHelperAssetType::BlueprintInterface:    return TEXT("blueprint_interface");
	case EBlueprintHelperAssetType::Structure:             return TEXT("structure");
	case EBlueprintHelperAssetType::InputAction:           return TEXT("input_action");
	case EBlueprintHelperAssetType::InputMappingContext:   return TEXT("input_mapping_context");
	case EBlueprintHelperAssetType::DataAsset:             return TEXT("data_asset");
	case EBlueprintHelperAssetType::DataTable:             return TEXT("data_table");
	case EBlueprintHelperAssetType::WidgetBlueprint:       return TEXT("widget_blueprint");
	case EBlueprintHelperAssetType::Material:              return TEXT("material");
	case EBlueprintHelperAssetType::Unknown:               return TEXT("unknown");
	default:                                               return TEXT("unknown");
	}
}

// ─── 工厂类型枚举 ───

/** UE 内部工厂 / 创建路径。 */
enum class EBlueprintHelperFactoryType : uint8
{
	Blueprint,
	BlueprintInterface,
	Structure,
	EnhancedInputAction,
	EnhancedInputMappingContext,
	DataAsset,
	DataTable,
	WidgetBlueprint,
	NativeFactory,
	Unknown
};

inline const TCHAR* FactoryTypeToString(EBlueprintHelperFactoryType Type)
{
	switch (Type)
	{
	case EBlueprintHelperFactoryType::Blueprint:                   return TEXT("blueprint");
	case EBlueprintHelperFactoryType::BlueprintInterface:          return TEXT("blueprint_interface");
	case EBlueprintHelperFactoryType::Structure:                   return TEXT("structure");
	case EBlueprintHelperFactoryType::EnhancedInputAction:         return TEXT("enhanced_input_action");
	case EBlueprintHelperFactoryType::EnhancedInputMappingContext: return TEXT("enhanced_input_mapping_context");
	case EBlueprintHelperFactoryType::DataAsset:                   return TEXT("data_asset");
	case EBlueprintHelperFactoryType::DataTable:                   return TEXT("data_table");
	case EBlueprintHelperFactoryType::WidgetBlueprint:             return TEXT("widget_blueprint");
	case EBlueprintHelperFactoryType::NativeFactory:               return TEXT("native_factory");
	case EBlueprintHelperFactoryType::Unknown:                     return TEXT("unknown");
	default:                                                       return TEXT("unknown");
	}
}

// ─── 冲突策略枚举 ───

/** 资产冲突处理策略。第一版只支持 fail_if_exists / reuse_if_exists。 */
enum class EBlueprintHelperAssetCollisionPolicy : uint8
{
	FailIfExists,
	ReuseIfExists
};

inline const TCHAR* AssetCollisionPolicyToString(EBlueprintHelperAssetCollisionPolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperAssetCollisionPolicy::FailIfExists:  return TEXT("fail_if_exists");
	case EBlueprintHelperAssetCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists");
	default:                                                  return TEXT("unknown");
	}
}

// ─── Input Action 值类型 ───

inline const TCHAR* InputActionValueTypeToString(const FString& ValueType)
{
	if (ValueType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))    return TEXT("boolean");
	if (ValueType.Equals(TEXT("axis1d"), ESearchCase::IgnoreCase))  return TEXT("axis1d");
	if (ValueType.Equals(TEXT("axis2d"), ESearchCase::IgnoreCase))  return TEXT("axis2d");
	if (ValueType.Equals(TEXT("axis3d"), ESearchCase::IgnoreCase))  return TEXT("axis3d");
	return TEXT("boolean");
}

#pragma region Asset Factory Structs

struct FBlueprintHelperAssetFactoryFieldSpec
{
	FString Name;
	FString Type;
	FString DefaultValue;
	bool bHasDefaultValue = false;

	FBlueprintHelperAssetFactoryFieldSpec() = default;

	FBlueprintHelperAssetFactoryFieldSpec(const FString& InName, const FString& InType)
		: Name(InName)
		, Type(InType)
	{
	}

	FBlueprintHelperAssetFactoryFieldSpec(const FString& InName, const FString& InType, const FString& InDefaultValue)
		: Name(InName)
		, Type(InType)
		, DefaultValue(InDefaultValue)
		, bHasDefaultValue(true)
	{
	}

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("type"), Type);
		if (bHasDefaultValue)
		{
			Json->SetStringField(TEXT("default_value"), DefaultValue);
		}
		return Json;
	}
};

// ─── 6.4 创建规格 ───

struct FBlueprintHelperAssetFactorySpec
{
	EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
	EBlueprintHelperFactoryType FactoryType = EBlueprintHelperFactoryType::Unknown;
	FString ParentClass;
	FString ValueType;
	FString RowStruct;
	FString DataAssetClass;
	TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_type"), AssetTypeToString(AssetType));
		Json->SetStringField(TEXT("factory_type"), FactoryTypeToString(FactoryType));
		if (!ParentClass.IsEmpty()) { Json->SetStringField(TEXT("parent_class"), ParentClass); }
		if (!ValueType.IsEmpty()) { Json->SetStringField(TEXT("value_type"), InputActionValueTypeToString(ValueType)); }
		if (!RowStruct.IsEmpty()) { Json->SetStringField(TEXT("row_struct"), RowStruct); }
		if (!DataAssetClass.IsEmpty()) { Json->SetStringField(TEXT("data_asset_class"), DataAssetClass); }
		if (Fields.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> FieldValues;
			FieldValues.Reserve(Fields.Num());
			for (const FBlueprintHelperAssetFactoryFieldSpec& Field : Fields)
			{
				FieldValues.Add(MakeShared<FJsonValueObject>(Field.ToJson()));
			}
			Json->SetArrayField(TEXT("fields"), FieldValues);
		}
		return Json;
	}
};

// ─── 6.5 创建结果摘要 ───

struct FBlueprintHelperCreatedAssetSummary
{
	FString AssetPath;
	FString AssetClass;
	bool bCreated = false;
	bool bAlreadyExisted = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("asset_class"), AssetClass);
		Json->SetBoolField(TEXT("created"), bCreated);
		Json->SetBoolField(TEXT("already_existed"), bAlreadyExisted);
		return Json;
	}
};

// ─── 6.6 冲突处理摘要 ───

struct FBlueprintHelperAssetCollisionSummary
{
	EBlueprintHelperAssetCollisionPolicy Policy = EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	bool bHandled = false;
	FString ExistingAssetPath;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("policy"), AssetCollisionPolicyToString(Policy));
		Json->SetBoolField(TEXT("handled"), bHandled);
		if (!ExistingAssetPath.IsEmpty()) { Json->SetStringField(TEXT("existing_asset_path"), ExistingAssetPath); }
		return Json;
	}
};

// ─── 6.3 AssetFactory Data ───

struct FBlueprintHelperAssetFactoryData
{
	static constexpr const TCHAR* SchemaString = TEXT("AssetFactory.v1");

	FBlueprintHelperAssetFactorySpec Factory;
	FBlueprintHelperCreatedAssetSummary Asset;
	FBlueprintHelperAssetCollisionSummary Collision;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);
		Json->SetObjectField(TEXT("factory"), Factory.ToJson());
		Json->SetObjectField(TEXT("asset"), Asset.ToJson());
		Json->SetObjectField(TEXT("collision"), Collision.ToJson());
		return Json;
	}
};

#pragma endregion Asset Factory Structs
