// BlueprintHelper Service Layer — DataTable 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperDataScope : uint8 { DataTable, DataTableRow, DataTableRows };
inline const TCHAR* DataScopeToString(EBlueprintHelperDataScope S)
{
	switch (S) { case EBlueprintHelperDataScope::DataTable: return TEXT("data_table"); case EBlueprintHelperDataScope::DataTableRow: return TEXT("data_table_row"); case EBlueprintHelperDataScope::DataTableRows: return TEXT("data_table_rows"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperDataReadScope : uint8 { DataTable, DataTableRows };
inline const TCHAR* DataReadScopeToString(EBlueprintHelperDataReadScope S)
{
	switch (S) { case EBlueprintHelperDataReadScope::DataTable: return TEXT("data_table"); case EBlueprintHelperDataReadScope::DataTableRows: return TEXT("data_table_rows"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperRowNameCollisionPolicy : uint8 { FailIfExists, ReuseIfExists };
inline const TCHAR* RowNameCollisionPolicyToString(EBlueprintHelperRowNameCollisionPolicy P)
{
	switch (P) { case EBlueprintHelperRowNameCollisionPolicy::FailIfExists: return TEXT("fail_if_exists"); case EBlueprintHelperRowNameCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperDataTableErrorCode : uint8
{
	InvalidRequest, AssetNotFound, TargetNotDataTable, RowStructMissing,
	RowNotFound, RowAlreadyExists, FieldNotFound, TypeMismatch,
	InvalidDataTableRowSettings, RowAddFailed, RowUpdateFailed, RowRemoveFailed, RollbackFailed
};
inline const TCHAR* DataTableErrorCodeToString(EBlueprintHelperDataTableErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperDataTableErrorCode::InvalidRequest:               return TEXT("invalid_request");
	case EBlueprintHelperDataTableErrorCode::AssetNotFound:                return TEXT("asset_not_found");
	case EBlueprintHelperDataTableErrorCode::TargetNotDataTable:           return TEXT("target_not_data_table");
	case EBlueprintHelperDataTableErrorCode::RowStructMissing:             return TEXT("row_struct_missing");
	case EBlueprintHelperDataTableErrorCode::RowNotFound:                  return TEXT("row_not_found");
	case EBlueprintHelperDataTableErrorCode::RowAlreadyExists:             return TEXT("row_already_exists");
	case EBlueprintHelperDataTableErrorCode::FieldNotFound:                return TEXT("field_not_found");
	case EBlueprintHelperDataTableErrorCode::TypeMismatch:                 return TEXT("type_mismatch");
	case EBlueprintHelperDataTableErrorCode::InvalidDataTableRowSettings:  return TEXT("invalid_data_table_row_settings");
	case EBlueprintHelperDataTableErrorCode::RowAddFailed:                 return TEXT("row_add_failed");
	case EBlueprintHelperDataTableErrorCode::RowUpdateFailed:              return TEXT("row_update_failed");
	case EBlueprintHelperDataTableErrorCode::RowRemoveFailed:              return TEXT("row_remove_failed");
	case EBlueprintHelperDataTableErrorCode::RollbackFailed:               return TEXT("rollback_failed");
	default:                                                                 return TEXT("unknown");
	}
}

// ─── ReadDataTable ───

struct FBlueprintHelperDataTableSummary
{
	FString RowStruct;
	int32 RowCount = 0;
	TArray<FString> RowNames;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("row_struct"), RowStruct);
		J->SetNumberField(TEXT("row_count"), RowCount);
		TArray<TSharedPtr<FJsonValue>> A; for (const auto& R : RowNames) A.Add(MakeShared<FJsonValueString>(R));
		J->SetArrayField(TEXT("row_names"), A);
		return J;
	}
};

struct FBlueprintHelperReadDataTableResultData
{
	FString Schema = TEXT("ReadDataTable.v1");
	FBlueprintHelperDataTableSummary DataTable;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("data_table"), DataTable.ToJson());
		return J;
	}
};

// ─── ReadDataTableRows ───

struct FBlueprintHelperDataTableRowValue
{
	FString RowName;
	TSharedPtr<FJsonObject> Values;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("row_name"), RowName);
		if (Values.IsValid()) J->SetObjectField(TEXT("values"), Values);
		return J;
	}
};

struct FBlueprintHelperReadDataTableRowsResultData
{
	FString Schema = TEXT("ReadDataTableRows.v1");
	TArray<FBlueprintHelperDataTableRowValue> Rows;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		TArray<TSharedPtr<FJsonValue>> A; for (const auto& R : Rows) A.Add(MakeShared<FJsonValueObject>(R.ToJson()));
		J->SetArrayField(TEXT("rows"), A);
		return J;
	}
};

// ─── RowResult（add/update/remove 共用） ───

struct FBlueprintHelperDataTableRowWriteResult
{
	TOptional<FString> Mode;
	TOptional<int32> RequestedCount, UpdatedCount, ChangedCount, NoOpCount;
	TOptional<int32> AddedCount;
	TOptional<bool> bReusedExisting;
	TOptional<int32> RemovedCount;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		if (Mode.IsSet()) J->SetStringField(TEXT("mode"), *Mode);
		if (RequestedCount.IsSet()) J->SetNumberField(TEXT("requested_count"), *RequestedCount);
		if (UpdatedCount.IsSet()) J->SetNumberField(TEXT("updated_count"), *UpdatedCount);
		if (ChangedCount.IsSet()) J->SetNumberField(TEXT("changed_count"), *ChangedCount);
		if (NoOpCount.IsSet()) J->SetNumberField(TEXT("no_op_count"), *NoOpCount);
		if (AddedCount.IsSet()) J->SetNumberField(TEXT("added_count"), *AddedCount);
		if (bReusedExisting.IsSet()) J->SetBoolField(TEXT("reused_existing"), *bReusedExisting);
		if (RemovedCount.IsSet()) J->SetNumberField(TEXT("removed_count"), *RemovedCount);
		return J;
	}
};

struct FBlueprintHelperDataTableRowWriteResultData
{
	FString Schema;
	FBlueprintHelperDataTableRowWriteResult RowResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("row_result"), RowResult.ToJson());
		return J;
	}
};
