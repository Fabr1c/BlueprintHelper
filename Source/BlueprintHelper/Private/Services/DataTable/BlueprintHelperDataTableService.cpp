// BlueprintHelper Service Layer 。DataTable 操作服务实现

#include "Services/DataTable/BlueprintHelperDataTableService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Engine/DataTable.h"
#include "DataTableEditorUtils.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// ═══════════════════════════════════════════════════════════
// 内部工具
// ═══════════════════════════════════════════════════════════

UDataTable* FBlueprintHelperDataTableService::ResolveDataTable(
	const FString& AssetPath, FString& OutError) const
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path 不能为空。");
		return nullptr;
	}

	UObject* Obj = StaticLoadObject(UDataTable::StaticClass(), nullptr, *AssetPath);
	UDataTable* DT = Cast<UDataTable>(Obj);
	if (!DT)
	{
		OutError = FString::Printf(TEXT("无法加载 DataTable: %s"), *AssetPath);
		return nullptr;
	}
	return DT;
}

TArray<FBlueprintHelperDataTableColumnInfo> FBlueprintHelperDataTableService::CollectColumns(
	const UScriptStruct* RowStruct)
{
	TArray<FBlueprintHelperDataTableColumnInfo> Columns;
	if (!RowStruct) return Columns;

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		FBlueprintHelperDataTableColumnInfo Col;
		Col.Name = Prop->GetName();
		Col.TypeName = Prop->GetCPPType();
		Columns.Add(MoveTemp(Col));
	}
	return Columns;
}

TMap<FString, FString> FBlueprintHelperDataTableService::ExportRowFields(
	const UScriptStruct* RowStruct,
	const uint8* RowData,
	UObject* Owner)
{
	TMap<FString, FString> Fields;
	if (!RowStruct || !RowData) return Fields;

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		FString ValueStr;
		Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Owner, PPF_None);
		Fields.Add(Prop->GetName(), MoveTemp(ValueStr));
	}
	return Fields;
}

bool FBlueprintHelperDataTableService::ApplyFieldsToRow(
	const UScriptStruct* RowStruct,
	uint8* RowData,
	const TMap<FString, FString>& Fields,
	UObject* Owner,
	FString& OutError)
{
	if (!RowStruct || !RowData)
	{
		OutError = TEXT("行结构或行数据为空。");
		return false;
	}

	for (const auto& Pair : Fields)
	{
		FProperty* Prop = RowStruct->FindPropertyByName(*Pair.Key);
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("行结构中未找到字段: %s"), *Pair.Key);
			return false;
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		const TCHAR* ImportResult = Prop->ImportText_Direct(*Pair.Value, ValuePtr, Owner, PPF_None);
		if (!ImportResult)
		{
			OutError = FString::Printf(TEXT("字段 %s 值导入失败: \"%s\""), *Pair.Key, *Pair.Value);
			return false;
		}
	}
	return true;
}

bool FBlueprintHelperDataTableService::ApplyFieldsToCandidateRow(
	const UScriptStruct* RowStruct,
	const uint8* SourceRowData,
	const TMap<FString, FString>& Fields,
	UObject* Owner,
	FString& OutError)
{
	if (!RowStruct)
	{
		OutError = TEXT("DataTable 没有关联的行结构体。");
		return false;
	}

	const int32 RowSize = RowStruct->GetStructureSize();
	TArray<uint8> CandidateRow;
	CandidateRow.SetNumZeroed(RowSize);
	uint8* CandidateData = CandidateRow.GetData();
	RowStruct->InitializeStruct(CandidateData);
	ON_SCOPE_EXIT
	{
		RowStruct->DestroyStruct(CandidateData);
	};

	if (SourceRowData)
	{
		RowStruct->CopyScriptStruct(CandidateData, SourceRowData);
	}

	return ApplyFieldsToRow(RowStruct, CandidateData, Fields, Owner, OutError);
}

// ═══════════════════════════════════════════════════════════
// GetDataTableRows
// ═══════════════════════════════════════════════════════════

FBlueprintHelperDataTableRowsResult FBlueprintHelperDataTableService::GetDataTableRows(
	const FString& AssetPath,
	const TArray<FString>& FilterRowNames) const
{
	FBlueprintHelperDataTableRowsResult Result;

	UDataTable* DT = ResolveDataTable(AssetPath, Result.ErrorMessage);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		Result.ErrorMessage = TEXT("DataTable 没有关联的行结构体。");
		return Result;
	}

	Result.RowStructName = RowStruct->GetName();
	Result.Columns = CollectColumns(RowStruct);

	// 构建过滤集合（空 = 全部）
	TSet<FName> FilterSet;
	for (const FString& N : FilterRowNames)
	{
		FilterSet.Add(FName(*N));
	}

	const TMap<FName, uint8*>& RowMap = DT->GetRowMap();
	for (const auto& Pair : RowMap)
	{
		if (FilterSet.Num() > 0 && !FilterSet.Contains(Pair.Key))
		{
			continue;
		}

		FBlueprintHelperDataTableRowInfo RowInfo;
		RowInfo.RowName = Pair.Key;
		RowInfo.Fields = ExportRowFields(RowStruct, Pair.Value, DT);
		Result.Rows.Add(MoveTemp(RowInfo));
	}

	Result.bSuccess = true;
	return Result;
}

// ═══════════════════════════════════════════════════════════
// AddDataTableRow
// ═══════════════════════════════════════════════════════════

FBlueprintHelperDataTableMutationResult FBlueprintHelperDataTableService::AddDataTableRow(
	const FString& AssetPath,
	const FString& RowName,
	const TMap<FString, FString>& Fields,
	bool bDryRun) const
{
	FBlueprintHelperDataTableMutationResult Result;
	Result.bDryRun = bDryRun;

	UDataTable* DT = ResolveDataTable(AssetPath, Result.ErrorMessage);
	if (!DT) return Result;

	const FName RowFName(*RowName);
	Result.AffectedRow = RowFName;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (bDryRun && !RowStruct)
	{
		Result.ErrorMessage = TEXT("DataTable 没有关联的行结构体。");
		return Result;
	}

	// 检查行是否已存在
	if (DT->FindRowUnchecked(RowFName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("。'%s' 已存在。"), *RowName);
		return Result;
	}

	if (!RowStruct)
	{
		Result.ErrorMessage = TEXT("DataTable 没有关联的行结构体。");
		return Result;
	}

	if (bDryRun)
	{
		FString FieldError;
		if (!ApplyFieldsToCandidateRow(RowStruct, nullptr, Fields, DT, FieldError))
		{
			Result.ErrorMessage = FieldError;
			return Result;
		}

		Result.bSuccess = true;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Add DataTable Row")), DT);

	// 使用 FDataTableEditorUtils 添加行（会自动初始化结构体）
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	uint8* NewRowData = FDataTableEditorUtils::AddRow(DT, RowFName);
	if (!NewRowData)
	{
		Result.ErrorMessage = FString::Printf(TEXT("添加。'%s' 失败。"), *RowName);
		FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Mutation.Rollback();
		return Result;
	}

	// 应用字段值
	if (Fields.Num() > 0)
	{
		FString FieldError;
		if (!ApplyFieldsToRow(RowStruct, NewRowData, Fields, DT, FieldError))
		{
			// 回滚：删除刚添加的行
			DT->RemoveRow(RowFName);
			Result.ErrorMessage = FieldError;
			FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			Mutation.Rollback();
			return Result;
		}
	}

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	Mutation.Commit();

	Result.bSuccess = true;
	return Result;
}

// ═══════════════════════════════════════════════════════════
// UpdateDataTableRow
// ═══════════════════════════════════════════════════════════

FBlueprintHelperDataTableMutationResult FBlueprintHelperDataTableService::UpdateDataTableRow(
	const FString& AssetPath,
	const FString& RowName,
	const TMap<FString, FString>& Fields,
	bool bDryRun) const
{
	FBlueprintHelperDataTableMutationResult Result;
	Result.bDryRun = bDryRun;

	UDataTable* DT = ResolveDataTable(AssetPath, Result.ErrorMessage);
	if (!DT) return Result;

	const FName RowFName(*RowName);
	Result.AffectedRow = RowFName;

	uint8* RowData = DT->FindRowUnchecked(RowFName);
	if (!RowData)
	{
		Result.ErrorMessage = FString::Printf(TEXT("未找到行: %s"), *RowName);
		return Result;
	}

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
	{
		Result.ErrorMessage = TEXT("DataTable 没有关联的行结构体。");
		return Result;
	}

	FString FieldError;
	const int32 RowSize = RowStruct->GetStructureSize();
	TArray<uint8> CandidateRow;
	CandidateRow.SetNumZeroed(RowSize);
	uint8* CandidateData = CandidateRow.GetData();
	RowStruct->InitializeStruct(CandidateData);
	ON_SCOPE_EXIT
	{
		RowStruct->DestroyStruct(CandidateData);
	};
	RowStruct->CopyScriptStruct(CandidateData, RowData);
	if (!ApplyFieldsToRow(RowStruct, CandidateData, Fields, DT, FieldError))
	{
		Result.ErrorMessage = FieldError;
		return Result;
	}

	if (bDryRun)
	{
		Result.bSuccess = true;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Update DataTable Row")), DT);
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	RowStruct->CopyScriptStruct(RowData, CandidateData);
	DT->HandleDataTableChanged(RowFName);
	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	Mutation.Commit();

	Result.bSuccess = true;
	return Result;
}

// ═══════════════════════════════════════════════════════════
// DeleteDataTableRow
// ═══════════════════════════════════════════════════════════

FBlueprintHelperDataTableMutationResult FBlueprintHelperDataTableService::DeleteDataTableRow(
	const FString& AssetPath,
	const FString& RowName,
	bool bDryRun) const
{
	FBlueprintHelperDataTableMutationResult Result;
	Result.bDryRun = bDryRun;

	UDataTable* DT = ResolveDataTable(AssetPath, Result.ErrorMessage);
	if (!DT) return Result;

	const FName RowFName(*RowName);
	Result.AffectedRow = RowFName;

	if (bDryRun && !DT->GetRowStruct())
	{
		Result.ErrorMessage = TEXT("DataTable 没有关联的行结构体。");
		return Result;
	}

	if (!DT->FindRowUnchecked(RowFName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("未找到行: %s"), *RowName);
		return Result;
	}

	if (bDryRun)
	{
		Result.bSuccess = true;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Delete DataTable Row")), DT);
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	if (!FDataTableEditorUtils::RemoveRow(DT, RowFName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("删除。'%s' 失败。"), *RowName);
		FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Mutation.Rollback();
		return Result;
	}

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	Mutation.Commit();

	Result.bSuccess = true;
	return Result;
}
