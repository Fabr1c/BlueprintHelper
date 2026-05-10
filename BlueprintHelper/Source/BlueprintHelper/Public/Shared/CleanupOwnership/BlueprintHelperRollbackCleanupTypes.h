// BlueprintHelper Service Layer 。RollbackCleanupTransaction 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

// ─── 枚举 ───

enum class EBlueprintHelperRollbackScope : uint8 { CleanupTransaction };

inline const TCHAR* RollbackScopeToString(EBlueprintHelperRollbackScope S)
{
	switch (S) { case EBlueprintHelperRollbackScope::CleanupTransaction: return TEXT("cleanup_transaction"); default: return TEXT("unknown"); }
}

inline bool ParseRollbackScope(const FString& Str, EBlueprintHelperRollbackScope& Out)
{
	if (Str.Equals(TEXT("cleanup_transaction"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperRollbackScope::CleanupTransaction; return true; }
	return false;
}

enum class EBlueprintHelperAlreadyRolledBackPolicy : uint8 { Error, Ignore };

inline const TCHAR* AlreadyRolledBackPolicyToString(EBlueprintHelperAlreadyRolledBackPolicy P)
{
	switch (P) { case EBlueprintHelperAlreadyRolledBackPolicy::Error: return TEXT("error"); case EBlueprintHelperAlreadyRolledBackPolicy::Ignore: return TEXT("ignore"); default: return TEXT("unknown"); }
}

inline bool ParseAlreadyRolledBackPolicy(const FString& S, EBlueprintHelperAlreadyRolledBackPolicy& Out)
{
	if (S.Equals(TEXT("error"), ESearchCase::IgnoreCase))  { Out = EBlueprintHelperAlreadyRolledBackPolicy::Error; return true; }
	if (S.Equals(TEXT("ignore"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperAlreadyRolledBackPolicy::Ignore; return true; }
	return false;
}

enum class EBlueprintHelperRollbackCleanupErrorCode : uint8
{
	TransactionNotFound, TransactionTypeMismatch, RollbackDataUnavailable, RollbackDataCompacted,
	AlreadyRolledBack, AssetNotFound, AssetStateConflict, GraphNotFound, GraphStateConflict,
	OwnershipConflict, RestoreNodeFailed, RestoreLinkFailed, RestoreMetadataFailed,
	JournalWriteFailed, WritePermissionDisabled, ProfilePolicyViolation, BridgeDisconnected,
	RollbackBlocked, RollbackFailed
};

inline const TCHAR* RollbackCleanupErrorCodeToString(EBlueprintHelperRollbackCleanupErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperRollbackCleanupErrorCode::TransactionNotFound:      return TEXT("transaction_not_found");
	case EBlueprintHelperRollbackCleanupErrorCode::TransactionTypeMismatch:  return TEXT("transaction_type_mismatch");
	case EBlueprintHelperRollbackCleanupErrorCode::RollbackDataUnavailable:  return TEXT("rollback_data_unavailable");
	case EBlueprintHelperRollbackCleanupErrorCode::RollbackDataCompacted:    return TEXT("rollback_data_compacted");
	case EBlueprintHelperRollbackCleanupErrorCode::AlreadyRolledBack:        return TEXT("already_rolled_back");
	case EBlueprintHelperRollbackCleanupErrorCode::AssetNotFound:            return TEXT("asset_not_found");
	case EBlueprintHelperRollbackCleanupErrorCode::AssetStateConflict:       return TEXT("asset_state_conflict");
	case EBlueprintHelperRollbackCleanupErrorCode::GraphNotFound:            return TEXT("graph_not_found");
	case EBlueprintHelperRollbackCleanupErrorCode::GraphStateConflict:       return TEXT("graph_state_conflict");
	case EBlueprintHelperRollbackCleanupErrorCode::OwnershipConflict:        return TEXT("ownership_conflict");
	case EBlueprintHelperRollbackCleanupErrorCode::RestoreNodeFailed:        return TEXT("restore_node_failed");
	case EBlueprintHelperRollbackCleanupErrorCode::RestoreLinkFailed:        return TEXT("restore_link_failed");
	case EBlueprintHelperRollbackCleanupErrorCode::RestoreMetadataFailed:    return TEXT("restore_metadata_failed");
	case EBlueprintHelperRollbackCleanupErrorCode::JournalWriteFailed:       return TEXT("journal_write_failed");
	case EBlueprintHelperRollbackCleanupErrorCode::WritePermissionDisabled:  return TEXT("write_permission_disabled");
	case EBlueprintHelperRollbackCleanupErrorCode::ProfilePolicyViolation:   return TEXT("profile_policy_violation");
	case EBlueprintHelperRollbackCleanupErrorCode::BridgeDisconnected:       return TEXT("bridge_disconnected");
	case EBlueprintHelperRollbackCleanupErrorCode::RollbackBlocked:          return TEXT("rollback_blocked");
	case EBlueprintHelperRollbackCleanupErrorCode::RollbackFailed:           return TEXT("rollback_failed");
	default:                                                                   return TEXT("unknown");
	}
}

// ─── RollbackSummary ───

struct FBlueprintHelperRollbackSummary
{
	int32 AffectedAssets = 0;
	int32 RestorableBlocks = 0;
	bool bRestorableNodesAvailable = false;
	bool bRollbackDataAvailable = false;
	bool bRestorableLinksAvailable = false;
	bool bAssetStateChecked = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("affected_assets"), AffectedAssets);
		J->SetNumberField(TEXT("restorable_blocks"), RestorableBlocks);
		J->SetBoolField(TEXT("restorable_nodes_available"), bRestorableNodesAvailable);
		J->SetBoolField(TEXT("rollback_data_available"), bRollbackDataAvailable);
		return J;
	}
};

// ─── 正式结果 ───

struct FBlueprintHelperRollbackCleanupResult
{
	FString RolledBackTransactionId;
	FString RollbackStatus; // succeeded | already_rolled_back
	FString AlreadyRolledBackPolicy;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("rolled_back_transaction_id"), RolledBackTransactionId);
		J->SetStringField(TEXT("rollback_status"), RollbackStatus);
		if (!AlreadyRolledBackPolicy.IsEmpty()) J->SetStringField(TEXT("already_rolled_back_policy"), AlreadyRolledBackPolicy);
		return J;
	}
};

struct FBlueprintHelperRollbackCleanupResultData
{
	FString Schema = TEXT("RollbackCleanupTransaction.v1");
	FBlueprintHelperRollbackCleanupResult RollbackResult;
	FBlueprintHelperWriteRef WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("rollback_result"), RollbackResult.ToJson());
		J->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		return J;
	}
};

// ─── DryRun ───

struct FBlueprintHelperRollbackDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	FBlueprintHelperRollbackSummary RollbackSummary;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperDryRunIssue> Conflicts;
	TArray<FBlueprintHelperDryRunIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRollbackCleanupDryRunData
{
	FString Schema = TEXT("RollbackCleanupTransactionDryRun.v1");
	FBlueprintHelperRollbackDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const;
};
