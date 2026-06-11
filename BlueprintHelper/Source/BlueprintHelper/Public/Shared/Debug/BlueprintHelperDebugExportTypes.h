// BlueprintHelper Service Layer — Debug Export 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperExportScope : uint8 { RuntimeDebug, EvidenceDebug, AssetSnapshot };
inline const TCHAR* ExportScopeToString(EBlueprintHelperExportScope S)
{
	switch (S) { case EBlueprintHelperExportScope::RuntimeDebug: return TEXT("runtime_debug"); case EBlueprintHelperExportScope::EvidenceDebug: return TEXT("evidence_debug"); case EBlueprintHelperExportScope::AssetSnapshot: return TEXT("asset_snapshot"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperSnapshotType : uint8
{
	LogicJson, RawJson, WidgetTree, ClassSettings, DataTableRows, DataAssetProperties
};
inline const TCHAR* SnapshotTypeToString(EBlueprintHelperSnapshotType S)
{
	switch (S)
	{
	case EBlueprintHelperSnapshotType::LogicJson:          return TEXT("logic_json");
	case EBlueprintHelperSnapshotType::RawJson:            return TEXT("raw_json");
	case EBlueprintHelperSnapshotType::WidgetTree:         return TEXT("widget_tree");
	case EBlueprintHelperSnapshotType::ClassSettings:      return TEXT("class_settings");
	case EBlueprintHelperSnapshotType::DataTableRows:      return TEXT("data_table_rows");
	case EBlueprintHelperSnapshotType::DataAssetProperties: return TEXT("data_asset_properties");
	default:                                                return TEXT("unknown");
	}
}

enum class EBlueprintHelperDebugExportErrorCode : uint8
{
	InvalidRequest, UnsupportedExportScope, UnsupportedSnapshotType,
	AssetNotFound, EvidenceNotFound, DebugBundleExportFailed,
	EvidenceDebugBundleExportFailed, AssetSnapshotExportFailed,
	ResourceRefNotFound, ResourceRefExpired, ResourceRefForbidden,
	SensitiveContentBlocked, InternalError
};
inline const TCHAR* DebugExportErrorCodeToString(EBlueprintHelperDebugExportErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperDebugExportErrorCode::InvalidRequest:                     return TEXT("invalid_request");
	case EBlueprintHelperDebugExportErrorCode::UnsupportedExportScope:             return TEXT("unsupported_export_scope");
	case EBlueprintHelperDebugExportErrorCode::UnsupportedSnapshotType:            return TEXT("unsupported_snapshot_type");
	case EBlueprintHelperDebugExportErrorCode::AssetNotFound:                      return TEXT("asset_not_found");
	case EBlueprintHelperDebugExportErrorCode::EvidenceNotFound:                return TEXT("evidence_not_found");
	case EBlueprintHelperDebugExportErrorCode::DebugBundleExportFailed:            return TEXT("debug_bundle_export_failed");
	case EBlueprintHelperDebugExportErrorCode::EvidenceDebugBundleExportFailed: return TEXT("evidence_debug_bundle_export_failed");
	case EBlueprintHelperDebugExportErrorCode::AssetSnapshotExportFailed:          return TEXT("asset_snapshot_export_failed");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefNotFound:                return TEXT("resource_ref_not_found");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefExpired:                 return TEXT("resource_ref_expired");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefForbidden:               return TEXT("resource_ref_forbidden");
	case EBlueprintHelperDebugExportErrorCode::SensitiveContentBlocked:            return TEXT("sensitive_content_blocked");
	case EBlueprintHelperDebugExportErrorCode::InternalError:                       return TEXT("internal_error");
	default:                                                                         return TEXT("unknown");
	}
}

// ─── ExportResult（三个导出工具共用） ───

struct FBlueprintHelperDebugExportResult
{
	bool bExported = false;
	TOptional<FString> BundleRef;
	TOptional<FString> SnapshotRef;
	FString Format;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("exported"), bExported);
		if (BundleRef.IsSet()) J->SetStringField(TEXT("bundle_ref"), *BundleRef);
		if (SnapshotRef.IsSet()) J->SetStringField(TEXT("snapshot_ref"), *SnapshotRef);
		J->SetStringField(TEXT("format"), Format);
		return J;
	}
};

struct FBlueprintHelperExportDebugBundleResultData
{
	FString Schema = TEXT("ExportDebugBundle.v1");
	FBlueprintHelperDebugExportResult ExportResult;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("export_result"), ExportResult.ToJson()); return J; }
};

struct FBlueprintHelperExportEvidenceDebugBundleResultData
{
	FString Schema = TEXT("ExportEvidenceDebugBundle.v1");
	FBlueprintHelperDebugExportResult ExportResult;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("export_result"), ExportResult.ToJson()); return J; }
};

struct FBlueprintHelperExportAssetLogicSnapshotResultData
{
	FString Schema = TEXT("ExportAssetLogicSnapshot.v1");
	FBlueprintHelperDebugExportResult ExportResult;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("export_result"), ExportResult.ToJson()); return J; }
};
