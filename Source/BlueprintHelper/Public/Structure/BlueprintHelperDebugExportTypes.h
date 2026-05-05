// BlueprintHelper Service Layer — Debug Export / Large Payload 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperExportScope : uint8 { RuntimeDebug, TransactionDebug, AssetSnapshot };
inline const TCHAR* ExportScopeToString(EBlueprintHelperExportScope S)
{
	switch (S) { case EBlueprintHelperExportScope::RuntimeDebug: return TEXT("runtime_debug"); case EBlueprintHelperExportScope::TransactionDebug: return TEXT("transaction_debug"); case EBlueprintHelperExportScope::AssetSnapshot: return TEXT("asset_snapshot"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperSnapshotType : uint8
{
	LogicMd, LogicJson, RawJson, WidgetTree, ClassSettings, DataTableRows, DataAssetProperties
};
inline const TCHAR* SnapshotTypeToString(EBlueprintHelperSnapshotType S)
{
	switch (S)
	{
	case EBlueprintHelperSnapshotType::LogicMd:            return TEXT("logic_md");
	case EBlueprintHelperSnapshotType::LogicJson:          return TEXT("logic_json");
	case EBlueprintHelperSnapshotType::RawJson:            return TEXT("raw_json");
	case EBlueprintHelperSnapshotType::WidgetTree:         return TEXT("widget_tree");
	case EBlueprintHelperSnapshotType::ClassSettings:      return TEXT("class_settings");
	case EBlueprintHelperSnapshotType::DataTableRows:      return TEXT("data_table_rows");
	case EBlueprintHelperSnapshotType::DataAssetProperties: return TEXT("data_asset_properties");
	default:                                                return TEXT("unknown");
	}
}

enum class EBlueprintHelperLargePayloadReadMode : uint8 { Summary, Chunk };
inline const TCHAR* LargePayloadReadModeToString(EBlueprintHelperLargePayloadReadMode M)
{
	switch (M) { case EBlueprintHelperLargePayloadReadMode::Summary: return TEXT("summary"); case EBlueprintHelperLargePayloadReadMode::Chunk: return TEXT("chunk"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperDebugExportErrorCode : uint8
{
	InvalidRequest, UnsupportedExportScope, UnsupportedSnapshotType,
	AssetNotFound, TransactionNotFound, DebugBundleExportFailed,
	TransactionDebugBundleExportFailed, AssetSnapshotExportFailed,
	ResourceRefNotFound, ResourceRefExpired, ResourceRefForbidden,
	ChunkIndexOutOfRange, PayloadReadFailed, SensitiveContentBlocked, InternalError
};
inline const TCHAR* DebugExportErrorCodeToString(EBlueprintHelperDebugExportErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperDebugExportErrorCode::InvalidRequest:                     return TEXT("invalid_request");
	case EBlueprintHelperDebugExportErrorCode::UnsupportedExportScope:             return TEXT("unsupported_export_scope");
	case EBlueprintHelperDebugExportErrorCode::UnsupportedSnapshotType:            return TEXT("unsupported_snapshot_type");
	case EBlueprintHelperDebugExportErrorCode::AssetNotFound:                      return TEXT("asset_not_found");
	case EBlueprintHelperDebugExportErrorCode::TransactionNotFound:                return TEXT("transaction_not_found");
	case EBlueprintHelperDebugExportErrorCode::DebugBundleExportFailed:            return TEXT("debug_bundle_export_failed");
	case EBlueprintHelperDebugExportErrorCode::TransactionDebugBundleExportFailed: return TEXT("transaction_debug_bundle_export_failed");
	case EBlueprintHelperDebugExportErrorCode::AssetSnapshotExportFailed:          return TEXT("asset_snapshot_export_failed");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefNotFound:                return TEXT("resource_ref_not_found");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefExpired:                 return TEXT("resource_ref_expired");
	case EBlueprintHelperDebugExportErrorCode::ResourceRefForbidden:               return TEXT("resource_ref_forbidden");
	case EBlueprintHelperDebugExportErrorCode::ChunkIndexOutOfRange:               return TEXT("chunk_index_out_of_range");
	case EBlueprintHelperDebugExportErrorCode::PayloadReadFailed:                   return TEXT("payload_read_failed");
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

struct FBlueprintHelperExportTransactionDebugBundleResultData
{
	FString Schema = TEXT("ExportTransactionDebugBundle.v1");
	FBlueprintHelperDebugExportResult ExportResult;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("export_result"), ExportResult.ToJson()); return J; }
};

struct FBlueprintHelperExportAssetLogicSnapshotResultData
{
	FString Schema = TEXT("ExportAssetLogicSnapshot.v1");
	FBlueprintHelperDebugExportResult ExportResult;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("export_result"), ExportResult.ToJson()); return J; }
};

// ─── ReadLargePayloadRef ───

struct FBlueprintHelperLargePayload
{
	TOptional<bool> bAvailable;
	FString Format;
	TOptional<int64> SizeBytes;
	int32 ChunkCount = 0;
	TOptional<int32> ChunkIndex;
	TOptional<FString> Content;
	TOptional<bool> bTruncated;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		if (bAvailable.IsSet()) J->SetBoolField(TEXT("available"), *bAvailable);
		J->SetStringField(TEXT("format"), Format);
		if (SizeBytes.IsSet()) J->SetNumberField(TEXT("size_bytes"), static_cast<double>(*SizeBytes));
		J->SetNumberField(TEXT("chunk_count"), ChunkCount);
		if (ChunkIndex.IsSet()) J->SetNumberField(TEXT("chunk_index"), *ChunkIndex);
		if (Content.IsSet()) J->SetStringField(TEXT("content"), *Content);
		if (bTruncated.IsSet()) J->SetBoolField(TEXT("truncated"), *bTruncated);
		return J;
	}
};

struct FBlueprintHelperReadLargePayloadRefResultData
{
	FString Schema = TEXT("ReadLargePayloadRef.v1");
	FBlueprintHelperLargePayload Payload;
	TSharedRef<FJsonObject> ToJson() const { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("schema"), Schema); J->SetObjectField(TEXT("payload"), Payload.ToJson()); return J; }
};
