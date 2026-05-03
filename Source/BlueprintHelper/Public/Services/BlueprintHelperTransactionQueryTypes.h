// BlueprintHelper Service Layer — Transaction Journal Query 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperTransactionQueryScope : uint8
{
	Asset, All, CleanupTransactions, GraphWriteTransactions,
	OwnershipTransactions, ReviewPending, RollbackAvailable, Transaction
};

inline const TCHAR* QueryScopeToString(EBlueprintHelperTransactionQueryScope S)
{
	switch (S)
	{
	case EBlueprintHelperTransactionQueryScope::Asset:                  return TEXT("asset");
	case EBlueprintHelperTransactionQueryScope::All:                    return TEXT("all");
	case EBlueprintHelperTransactionQueryScope::CleanupTransactions:    return TEXT("cleanup_transactions");
	case EBlueprintHelperTransactionQueryScope::GraphWriteTransactions: return TEXT("graph_write_transactions");
	case EBlueprintHelperTransactionQueryScope::OwnershipTransactions:  return TEXT("ownership_transactions");
	case EBlueprintHelperTransactionQueryScope::ReviewPending:          return TEXT("review_pending");
	case EBlueprintHelperTransactionQueryScope::RollbackAvailable:      return TEXT("rollback_available");
	case EBlueprintHelperTransactionQueryScope::Transaction:            return TEXT("transaction");
	default:                                                             return TEXT("unknown");
	}
}

enum class EBlueprintHelperTransactionDetailLevel : uint8 { Summary, Debug };

inline const TCHAR* DetailLevelToString(EBlueprintHelperTransactionDetailLevel D)
{
	switch (D) { case EBlueprintHelperTransactionDetailLevel::Summary: return TEXT("summary"); case EBlueprintHelperTransactionDetailLevel::Debug: return TEXT("debug"); default: return TEXT("unknown"); }
}

// ─── List 结果 ───

struct FBlueprintHelperTransactionListItem
{
	FString TransactionId, Operation, Status;
	int32 AssetCount = 0;
	FString ReviewStatus;
	bool bRollbackAvailable = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("transaction_id"), TransactionId);
		J->SetStringField(TEXT("operation"), Operation);
		J->SetStringField(TEXT("status"), Status);
		J->SetNumberField(TEXT("asset_count"), AssetCount);
		J->SetStringField(TEXT("review_status"), ReviewStatus);
		J->SetBoolField(TEXT("rollback_available"), bRollbackAvailable);
		return J;
	}
};

struct FBlueprintHelperTransactionPageInfo
{
	int32 Limit = 20;
	bool bHasMore = false;
	TOptional<FString> NextCursor;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("limit"), Limit);
		J->SetBoolField(TEXT("has_more"), bHasMore);
		if (NextCursor.IsSet()) J->SetStringField(TEXT("next_cursor"), *NextCursor);
		return J;
	}
};

struct FBlueprintHelperListTransactionsResultData
{
	FString Schema = TEXT("ListBlueprintHelperTransactions.v1");
	TArray<FBlueprintHelperTransactionListItem> Transactions;
	FBlueprintHelperTransactionPageInfo Page;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		TArray<TSharedPtr<FJsonValue>> A;
		for (const auto& T : Transactions) A.Add(MakeShared<FJsonValueObject>(T.ToJson()));
		J->SetArrayField(TEXT("transactions"), A);
		J->SetObjectField(TEXT("page"), Page.ToJson());
		return J;
	}
};

// ─── Read 结果 ───

struct FBlueprintHelperTransactionTargetSummary
{
	FString AssetPath;
	TOptional<FString> GraphName;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("asset_path"), AssetPath);
		if (GraphName.IsSet()) J->SetStringField(TEXT("graph"), *GraphName);
		return J;
	}
};

struct FBlueprintHelperTransactionOperationSummary
{
	TOptional<int32> AffectedAssets;
	TOptional<int32> AffectedOwnedBlocks;
	TOptional<int32> ConvertedCount;
	TOptional<int32> CleanedCount;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		if (AffectedAssets.IsSet()) J->SetNumberField(TEXT("affected_assets"), *AffectedAssets);
		if (AffectedOwnedBlocks.IsSet()) J->SetNumberField(TEXT("affected_owned_blocks"), *AffectedOwnedBlocks);
		if (ConvertedCount.IsSet()) J->SetNumberField(TEXT("converted_count"), *ConvertedCount);
		if (CleanedCount.IsSet()) J->SetNumberField(TEXT("cleaned_count"), *CleanedCount);
		return J;
	}
};

struct FBlueprintHelperTransactionSummaryRecord
{
	FString TransactionId, Operation, Status;
	FString ReviewStatus;
	bool bRollbackAvailable = false;
	TArray<FBlueprintHelperTransactionTargetSummary> Targets;
	FBlueprintHelperTransactionOperationSummary Summary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("transaction_id"), TransactionId);
		J->SetStringField(TEXT("operation"), Operation);
		J->SetStringField(TEXT("status"), Status);
		J->SetStringField(TEXT("review_status"), ReviewStatus);
		J->SetBoolField(TEXT("rollback_available"), bRollbackAvailable);
		TArray<TSharedPtr<FJsonValue>> A;
		for (const auto& T : Targets) A.Add(MakeShared<FJsonValueObject>(T.ToJson()));
		J->SetArrayField(TEXT("targets"), A);
		J->SetObjectField(TEXT("summary"), Summary.ToJson());
		return J;
	}
};

struct FBlueprintHelperReadTransactionResultData
{
	FString Schema = TEXT("ReadBlueprintHelperTransaction.v1");
	FBlueprintHelperTransactionSummaryRecord Transaction;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("transaction"), Transaction.ToJson());
		return J;
	}
};
