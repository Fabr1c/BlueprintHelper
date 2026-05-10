// BlueprintHelper Service Layer 。CleanupBlueprintHelperBlock 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

enum class EBlueprintHelperCleanupScope : uint8 { Block };

inline const TCHAR* CleanupScopeToString(EBlueprintHelperCleanupScope Scope)
{
	switch (Scope) { case EBlueprintHelperCleanupScope::Block: return TEXT("block"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperMissingPolicy : uint8 { Error, Ignore };

inline const TCHAR* MissingPolicyToString(EBlueprintHelperMissingPolicy P)
{
	switch (P) { case EBlueprintHelperMissingPolicy::Error: return TEXT("error"); case EBlueprintHelperMissingPolicy::Ignore: return TEXT("ignore"); default: return TEXT("unknown"); }
}

inline bool ParseMissingPolicy(const FString& S, EBlueprintHelperMissingPolicy& Out)
{
	if (S.Equals(TEXT("error"), ESearchCase::IgnoreCase))  { Out = EBlueprintHelperMissingPolicy::Error; return true; }
	if (S.Equals(TEXT("ignore"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperMissingPolicy::Ignore; return true; }
	return false;
}

enum class EBlueprintHelperCleanupStage : uint8
{
	ParseInput, ResolveTarget, ResolveGraph, ResolveBlock, OwnershipCheck,
	DependencyCheck, DryRun, Snapshot, DeleteLinks, DeleteNodes, WriteJournal, Rollback
};

inline const TCHAR* CleanupStageToString(EBlueprintHelperCleanupStage S)
{
	switch (S)
	{
	case EBlueprintHelperCleanupStage::ParseInput:       return TEXT("parse_input");
	case EBlueprintHelperCleanupStage::ResolveTarget:    return TEXT("resolve_target");
	case EBlueprintHelperCleanupStage::ResolveGraph:     return TEXT("resolve_graph");
	case EBlueprintHelperCleanupStage::ResolveBlock:     return TEXT("resolve_block");
	case EBlueprintHelperCleanupStage::OwnershipCheck:   return TEXT("ownership_check");
	case EBlueprintHelperCleanupStage::DependencyCheck:  return TEXT("dependency_check");
	case EBlueprintHelperCleanupStage::DryRun:           return TEXT("dry_run");
	case EBlueprintHelperCleanupStage::Snapshot:         return TEXT("snapshot");
	case EBlueprintHelperCleanupStage::DeleteLinks:      return TEXT("delete_links");
	case EBlueprintHelperCleanupStage::DeleteNodes:      return TEXT("delete_nodes");
	case EBlueprintHelperCleanupStage::WriteJournal:     return TEXT("write_journal");
	case EBlueprintHelperCleanupStage::Rollback:         return TEXT("rollback");
	default:                                             return TEXT("unknown");
	}
}

enum class EBlueprintHelperCleanupErrorCode : uint8
{
	InvalidRequest, TargetBlueprintNotFound, TargetNotBlueprint, TargetGraphNotFound,
	BlockNotFound, TargetNotOwned, OwnershipMismatch, ExternalDependentsExist,
	NodeDeleteFailed, LinkDeleteFailed, JournalWriteFailed, RollbackBlocked,
	RollbackFailed, WritePermissionDisabled, ProfilePolicyViolation, BridgeDisconnected
};

inline const TCHAR* CleanupErrorCodeToString(EBlueprintHelperCleanupErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperCleanupErrorCode::InvalidRequest:            return TEXT("invalid_request");
	case EBlueprintHelperCleanupErrorCode::TargetBlueprintNotFound:   return TEXT("target_blueprint_not_found");
	case EBlueprintHelperCleanupErrorCode::TargetNotBlueprint:        return TEXT("target_not_blueprint");
	case EBlueprintHelperCleanupErrorCode::TargetGraphNotFound:       return TEXT("target_graph_not_found");
	case EBlueprintHelperCleanupErrorCode::BlockNotFound:             return TEXT("block_not_found");
	case EBlueprintHelperCleanupErrorCode::TargetNotOwned:            return TEXT("target_not_owned");
	case EBlueprintHelperCleanupErrorCode::OwnershipMismatch:         return TEXT("ownership_mismatch");
	case EBlueprintHelperCleanupErrorCode::ExternalDependentsExist:    return TEXT("external_dependents_exist");
	case EBlueprintHelperCleanupErrorCode::NodeDeleteFailed:          return TEXT("node_delete_failed");
	case EBlueprintHelperCleanupErrorCode::LinkDeleteFailed:          return TEXT("link_delete_failed");
	case EBlueprintHelperCleanupErrorCode::JournalWriteFailed:        return TEXT("journal_write_failed");
	case EBlueprintHelperCleanupErrorCode::RollbackBlocked:           return TEXT("rollback_blocked");
	case EBlueprintHelperCleanupErrorCode::RollbackFailed:            return TEXT("rollback_failed");
	case EBlueprintHelperCleanupErrorCode::WritePermissionDisabled:   return TEXT("write_permission_disabled");
	case EBlueprintHelperCleanupErrorCode::ProfilePolicyViolation:    return TEXT("profile_policy_violation");
	case EBlueprintHelperCleanupErrorCode::BridgeDisconnected:        return TEXT("bridge_disconnected");
	default:                                                           return TEXT("unknown");
	}
}

// ─── 结果结构 ───

struct FBlueprintHelperCleanedRef
{
	FString GraphId;
	FString BlockRef;
	FString BlockId; // fallback

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("graph_id"), GraphId);
		J->SetStringField(TEXT("block_ref"), BlockRef);
		return J;
	}
};

struct FBlueprintHelperCleanupBlockResult
{
	FBlueprintHelperCleanedRef CleanedRef;
	TOptional<FString> MissingPolicy;
	TOptional<bool> bMissing;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetObjectField(TEXT("cleaned_ref"), CleanedRef.ToJson());
		if (MissingPolicy.IsSet()) J->SetStringField(TEXT("missing_policy"), *MissingPolicy);
		if (bMissing.IsSet()) J->SetBoolField(TEXT("missing"), *bMissing);
		return J;
	}
};

struct FBlueprintHelperCleanupBlockResultData
{
	FString Schema = TEXT("CleanupBlueprintHelperBlock.v1");
	FBlueprintHelperCleanupBlockResult CleanupResult;
	TOptional<FBlueprintHelperWriteRef> WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("cleanup_result"), CleanupResult.ToJson());
		if (WriteRef.IsSet()) J->SetObjectField(TEXT("write_ref"), WriteRef->ToJson());
		return J;
	}
};

// ─── DryRun ───

struct FBlueprintHelperCleanupDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperDryRunIssue> Conflicts;
	TArray<FBlueprintHelperDryRunIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperCleanupDryRunData
{
	FString Schema = TEXT("CleanupBlueprintHelperBlockDryRun.v1");
	FBlueprintHelperCleanupDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const;
};
