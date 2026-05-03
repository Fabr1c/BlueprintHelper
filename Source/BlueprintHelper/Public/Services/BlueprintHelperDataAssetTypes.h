// BlueprintHelper Service Layer — DataAsset 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperDataAssetScope : uint8 { DataAssetProperty };
inline const TCHAR* DataAssetScopeToString(EBlueprintHelperDataAssetScope S)
{
	switch (S) { case EBlueprintHelperDataAssetScope::DataAssetProperty: return TEXT("data_asset_property"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperDataAssetReadScope : uint8 { DataAssetProperties };
inline const TCHAR* DataAssetReadScopeToString(EBlueprintHelperDataAssetReadScope S)
{
	switch (S) { case EBlueprintHelperDataAssetReadScope::DataAssetProperties: return TEXT("data_asset_properties"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperPropertyWriteMode : uint8 { Single, Batch };
inline const TCHAR* PropertyWriteModeToString(EBlueprintHelperPropertyWriteMode M)
{
	switch (M) { case EBlueprintHelperPropertyWriteMode::Single: return TEXT("single"); case EBlueprintHelperPropertyWriteMode::Batch: return TEXT("batch"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperDataAssetErrorCode : uint8
{
	InvalidRequest, AssetNotFound, TargetNotDataAsset, UnsupportedAssetClass,
	PropertyNotFound, PropertyNotWritable, TypeMismatch, UnsupportedPropertyType,
	InvalidDataAssetPropertySettings, PropertySetFailed, RollbackFailed, InternalError
};
inline const TCHAR* DataAssetErrorCodeToString(EBlueprintHelperDataAssetErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperDataAssetErrorCode::InvalidRequest:                   return TEXT("invalid_request");
	case EBlueprintHelperDataAssetErrorCode::AssetNotFound:                    return TEXT("asset_not_found");
	case EBlueprintHelperDataAssetErrorCode::TargetNotDataAsset:               return TEXT("target_not_data_asset");
	case EBlueprintHelperDataAssetErrorCode::UnsupportedAssetClass:            return TEXT("unsupported_asset_class");
	case EBlueprintHelperDataAssetErrorCode::PropertyNotFound:                 return TEXT("property_not_found");
	case EBlueprintHelperDataAssetErrorCode::PropertyNotWritable:              return TEXT("property_not_writable");
	case EBlueprintHelperDataAssetErrorCode::TypeMismatch:                     return TEXT("type_mismatch");
	case EBlueprintHelperDataAssetErrorCode::UnsupportedPropertyType:          return TEXT("unsupported_property_type");
	case EBlueprintHelperDataAssetErrorCode::InvalidDataAssetPropertySettings: return TEXT("invalid_data_asset_property_settings");
	case EBlueprintHelperDataAssetErrorCode::PropertySetFailed:                return TEXT("property_set_failed");
	case EBlueprintHelperDataAssetErrorCode::RollbackFailed:                   return TEXT("rollback_failed");
	case EBlueprintHelperDataAssetErrorCode::InternalError:                    return TEXT("internal_error");
	default:                                                                     return TEXT("unknown");
	}
}

// ─── ReadDataAssetProperties ───

struct FBlueprintHelperDataAssetProperties
{
	FString AssetClass;
	int32 PropertyCount = 0;
	TSharedPtr<FJsonObject> Values;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("asset_class"), AssetClass);
		J->SetNumberField(TEXT("property_count"), PropertyCount);
		if (Values.IsValid()) J->SetObjectField(TEXT("values"), Values);
		return J;
	}
};

struct FBlueprintHelperReadDataAssetPropertiesResultData
{
	FString Schema = TEXT("ReadDataAssetProperties.v1");
	FBlueprintHelperDataAssetProperties Properties;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("properties"), Properties.ToJson());
		return J;
	}
};

// ─── PropertyWriteResult（DataAsset 属性写入共用） ───

struct FBlueprintHelperDataAssetPropertyWriteResult
{
	FString Mode;
	int32 RequestedCount = 0, AppliedCount = 0, ChangedCount = 0, NoOpCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("mode"), Mode);
		J->SetNumberField(TEXT("requested_count"), RequestedCount);
		J->SetNumberField(TEXT("applied_count"), AppliedCount);
		J->SetNumberField(TEXT("changed_count"), ChangedCount);
		J->SetNumberField(TEXT("no_op_count"), NoOpCount);
		return J;
	}
};

struct FBlueprintHelperSetDataAssetPropertyResultData
{
	FString Schema = TEXT("SetDataAssetProperty.v1");
	FBlueprintHelperDataAssetPropertyWriteResult PropertyResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("property_result"), PropertyResult.ToJson());
		return J;
	}
};
