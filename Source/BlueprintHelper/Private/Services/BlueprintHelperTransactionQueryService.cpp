// BlueprintHelper Service Layer — Transaction Query 服务实现

#include "Services/BlueprintHelperTransactionQueryService.h"
#include "Services/BlueprintHelperToolResultTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"

FBlueprintHelperToolResultBase FBlueprintHelperTransactionQueryService::List(const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FListRequest Req = ParseListRequest(Payload);

	TArray<FBlueprintHelperTransactionListItem> AllItems;
	LoadJournalIndex(AllItems);

	// Filter
	TArray<FBlueprintHelperTransactionListItem> Filtered;
	FString Scope = Req.QueryScope.IsEmpty() ? TEXT("all") : Req.QueryScope;
	for (auto& Item : AllItems)
	{
		if (Scope == TEXT("asset") && !Req.AssetPath.IsEmpty())
		{
			// Simple check: the item might match by tool prefix
			if (!Item.Operation.Contains(TEXT("cleanup")) && !Item.Operation.Contains(TEXT("convert")))
				continue;
		}
		if (Scope == TEXT("cleanup_transactions") && !Item.Operation.Contains(TEXT("cleanup")))
			continue;
		if (Scope == TEXT("review_pending") && !Item.ReviewStatus.Contains(TEXT("pending")))
			continue;
		Filtered.Add(Item);
	}

	const int32 Total = Filtered.Num();
	const int32 Offset = 0;
	const int32 Limit = FMath::Clamp(Req.Limit, 1, 100);
	const bool bHasMore = Offset + Limit < Total;

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
	Result.Operation = TEXT("list_blueprint_helper_transactions");
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;

	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("query_scope"), Scope);
	if (!Req.AssetPath.IsEmpty()) Tgt->SetStringField(TEXT("asset_path"), Req.AssetPath);
	Result.CustomTargetJson = Tgt;

	FBlueprintHelperListTransactionsResultData Data;
	for (int32 i = Offset; i < FMath::Min(Offset + Limit, Total); ++i)
		Data.Transactions.Add(Filtered[i]);
	Data.Page.Limit = Limit;
	Data.Page.bHasMore = bHasMore;
	Result.Data = Data.ToJson();
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTransactionQueryService::Read(const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FReadRequest Req = ParseReadRequest(Payload);

	if (Req.TransactionId.IsEmpty())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_helper_transaction"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 transaction_id。"), false});

	if (Req.DetailLevel == TEXT("debug"))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_helper_transaction"), TraceId,
			{TEXT("unsupported_detail_level"), EBlueprintHelperToolStage::ParseInput, TEXT("detail_level=debug 暂不支持。"), false});

	TSharedPtr<FJsonObject> Record;
	if (!ReadJournalFile(Req.TransactionId, Record))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_helper_transaction"), TraceId,
			{TEXT("transaction_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 FString::Printf(TEXT("Transaction %s 未找到。"), *Req.TransactionId), false});

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
	Result.Operation = TEXT("read_blueprint_helper_transaction");
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;

	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("transaction_id"), Req.TransactionId);
	Tgt->SetStringField(TEXT("query_scope"), TEXT("transaction"));
	Tgt->SetStringField(TEXT("detail_level"), TEXT("summary"));
	Result.CustomTargetJson = Tgt;

	FBlueprintHelperReadTransactionResultData Data;
	BuildReadSummary(Record, Data.Transaction);
	Result.Data = Data.ToJson();
	return Result;
}

// ─── Parse ───

FBlueprintHelperTransactionQueryService::FListRequest
FBlueprintHelperTransactionQueryService::ParseListRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FListRequest R;
	if (!Payload.IsValid()) return R;
	Payload->TryGetStringField(TEXT("query_scope"), R.QueryScope);
	Payload->TryGetStringField(TEXT("asset_path"), R.AssetPath);
	int32 L = 20; if (Payload->TryGetNumberField(TEXT("limit"), L)) R.Limit = L;
	return R;
}

FBlueprintHelperTransactionQueryService::FReadRequest
FBlueprintHelperTransactionQueryService::ParseReadRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FReadRequest R;
	if (!Payload.IsValid()) return R;
	Payload->TryGetStringField(TEXT("transaction_id"), R.TransactionId);
	Payload->TryGetStringField(TEXT("detail_level"), R.DetailLevel);
	return R;
}

// ─── Journal 扫描 ───

bool FBlueprintHelperTransactionQueryService::LoadJournalIndex(
	TArray<FBlueprintHelperTransactionListItem>& OutItems) const
{
	const FString JournalDir = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Transactions") / TEXT("Active");
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(JournalDir / TEXT("*.json")), true, false);

	for (const FString& File : Files)
	{
		const FString Path = JournalDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path)) continue;

		TSharedPtr<FJsonObject> Rec;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Rec) || !Rec.IsValid()) continue;

		FBlueprintHelperTransactionListItem Item;
		Rec->TryGetStringField(TEXT("transaction_id"), Item.TransactionId);
		Rec->TryGetStringField(TEXT("tool"), Item.Operation);
		Rec->TryGetStringField(TEXT("status"), Item.Status);
		if (Item.Status.IsEmpty()) Rec->TryGetStringField(TEXT("status"), Item.Status);

		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		if (Rec->TryGetArrayField(TEXT("target_assets"), Assets)) Item.AssetCount = Assets->Num();

		// review_status
		FString RStatus;
		Rec->TryGetStringField(TEXT("review_status"), RStatus);
		Item.ReviewStatus = RStatus.IsEmpty() ? TEXT("unknown") : RStatus;

		// rollback_available: has rollback_data
		const TSharedPtr<FJsonObject>* Rd = nullptr;
		Item.bRollbackAvailable = Rec->TryGetObjectField(TEXT("rollback_data"), Rd) && Rd->IsValid();

		if (!Item.TransactionId.IsEmpty())
			OutItems.Add(Item);
	}
	return true;
}

bool FBlueprintHelperTransactionQueryService::ReadJournalFile(
	const FString& TransactionId, TSharedPtr<FJsonObject>& OutRecord) const
{
	const FString JournalDir = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Transactions") / TEXT("Active");
	const FString Path = JournalDir / FString::Printf(TEXT("%s.json"), *TransactionId);

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		const FString ReviewDir = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Review");
		const FString RPath = ReviewDir / FString::Printf(TEXT("%s.json"), *TransactionId);
		if (!FFileHelper::LoadFileToString(Content, *RPath)) return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	return FJsonSerializer::Deserialize(Reader, OutRecord) && OutRecord.IsValid();
}

void FBlueprintHelperTransactionQueryService::BuildReadSummary(
	const TSharedPtr<FJsonObject>& Record, FBlueprintHelperTransactionSummaryRecord& Out) const
{
	Record->TryGetStringField(TEXT("transaction_id"), Out.TransactionId);
	Record->TryGetStringField(TEXT("tool"), Out.Operation);
	Record->TryGetStringField(TEXT("status"), Out.Status);

	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	if (Record->TryGetArrayField(TEXT("target_assets"), Assets))
	{
		for (const auto& V : *Assets)
		{
			FString Asset; if (V->TryGetString(Asset))
			{
				FBlueprintHelperTransactionTargetSummary Tgt; Tgt.AssetPath = Asset;
				Out.Targets.Add(Tgt);
			}
		}
	}

	FString RS;
	Record->TryGetStringField(TEXT("review_status"), RS);
	Out.ReviewStatus = RS.IsEmpty() ? TEXT("unknown") : RS;

	const TSharedPtr<FJsonObject>* Rd = nullptr;
	Out.bRollbackAvailable = Record->TryGetObjectField(TEXT("rollback_data"), Rd) && Rd->IsValid();

	// Summary
	Out.Summary.AffectedAssets = Assets ? Assets->Num() : 0;

	const TArray<TSharedPtr<FJsonValue>>* Blocks = nullptr;
	if (Record->TryGetArrayField(TEXT("blocks"), Blocks))
		Out.Summary.AffectedOwnedBlocks = Blocks->Num();
}
