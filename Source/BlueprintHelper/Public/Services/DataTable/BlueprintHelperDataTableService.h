// BlueprintHelper Service Layer — DataTable 操作服务

#pragma once

#include "CoreMinimal.h"

class UDataTable;

// ─── DataTable Schema 信息 ───

/** DataTable 列（行结构属性）的描述。 */
struct BLUEPRINTHELPER_API FBlueprintHelperDataTableColumnInfo
{
	FString Name;
	FString TypeName;
};

// ─── DataTable 行信息 ───

/** 单行数据，属性名 → 文本值。 */
struct BLUEPRINTHELPER_API FBlueprintHelperDataTableRowInfo
{
	FName RowName;
	TMap<FString, FString> Fields;
};

// ─── 查询结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperDataTableRowsResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString RowStructName;
	TArray<FBlueprintHelperDataTableColumnInfo> Columns;
	TArray<FBlueprintHelperDataTableRowInfo> Rows;
};

struct BLUEPRINTHELPER_API FBlueprintHelperDataTableMutationResult
{
	bool bSuccess = false;
	bool bDryRun = false;
	FString ErrorMessage;
	FName AffectedRow;
};

/**
 * DataTable CRUD 操作服务。
 * 通过 FProperty 反射对行数据进行读写。
 */
class BLUEPRINTHELPER_API FBlueprintHelperDataTableService
{
public:
	/** 获取 DataTable 的行数据（可选过滤行名）。 */
	FBlueprintHelperDataTableRowsResult GetDataTableRows(
		const FString& AssetPath,
		const TArray<FString>& FilterRowNames = TArray<FString>()) const;

	/** 添加新行（字段通过 TMap 传入）。 */
	FBlueprintHelperDataTableMutationResult AddDataTableRow(
		const FString& AssetPath,
		const FString& RowName,
		const TMap<FString, FString>& Fields,
		bool bDryRun = false) const;

	/** 更新已有行的字段。 */
	FBlueprintHelperDataTableMutationResult UpdateDataTableRow(
		const FString& AssetPath,
		const FString& RowName,
		const TMap<FString, FString>& Fields,
		bool bDryRun = false) const;

	/** 删除行。 */
	FBlueprintHelperDataTableMutationResult DeleteDataTableRow(
		const FString& AssetPath,
		const FString& RowName,
		bool bDryRun = false) const;

private:
	/** 根据资产路径加载 DataTable。 */
	UDataTable* ResolveDataTable(const FString& AssetPath, FString& OutError) const;

	/** 将字段值写入行数据。 */
	static bool ApplyFieldsToRow(
		const UScriptStruct* RowStruct,
		uint8* RowData,
		const TMap<FString, FString>& Fields,
		UObject* Owner,
		FString& OutError);

	/** 将字段值应用到临时候选行，用于 dry-run 和预校验。 */
	static bool ApplyFieldsToCandidateRow(
		const UScriptStruct* RowStruct,
		const uint8* SourceRowData,
		const TMap<FString, FString>& Fields,
		UObject* Owner,
		FString& OutError);

	/** 收集行结构的列信息。 */
	static TArray<FBlueprintHelperDataTableColumnInfo> CollectColumns(const UScriptStruct* RowStruct);

	/** 从行数据中导出所有字段为文本。 */
	static TMap<FString, FString> ExportRowFields(
		const UScriptStruct* RowStruct,
		const uint8* RowData,
		UObject* Owner);
};
