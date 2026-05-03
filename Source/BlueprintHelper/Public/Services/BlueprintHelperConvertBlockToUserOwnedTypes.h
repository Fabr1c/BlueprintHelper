// BlueprintHelper Service Layer — ConvertBlockToUserOwned 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Services/BlueprintHelperAppendGraphTypes.h"

enum class EBlueprintHelperOwnershipScope : uint8 { Block };

inline const TCHAR* OwnershipScopeToString(EBlueprintHelperOwnershipScope S)
{
	switch (S) { case EBlueprintHelperOwnershipScope::Block: return TEXT("block"); default: return TEXT("unknown"); }
}

inline bool ParseOwnershipScope(const FString& Str, EBlueprintHelperOwnershipScope& Out)
{
	if (Str.Equals(TEXT("block"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperOwnershipScope::Block; return true; }
	return false;
}

enum class EBlueprintHelperAlreadyUserOwnedPolicy : uint8 { Error, Ignore };

inline const TCHAR* AlreadyUserOwnedPolicyToString(EBlueprintHelperAlreadyUserOwnedPolicy P)
{
	switch (P) { case EBlueprintHelperAlreadyUserOwnedPolicy::Error: return TEXT("error"); case EBlueprintHelperAlreadyUserOwnedPolicy::Ignore: return TEXT("ignore"); default: return TEXT("unknown"); }
}

inline bool ParseAlreadyUserOwnedPolicy(const FString& S, EBlueprintHelperAlreadyUserOwnedPolicy& Out)
{
	if (S.Equals(TEXT("error"), ESearchCase::IgnoreCase))  { Out = EBlueprintHelperAlreadyUserOwnedPolicy::Error; return true; }
	if (S.Equals(TEXT("ignore"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperAlreadyUserOwnedPolicy::Ignore; return true; }
	return false;
}

enum class EBlueprintHelperOwnershipErrorCode : uint8
{
	InvalidRequest, UnsupportedOwnershipScope, TargetBlueprintNotFound, TargetGraphNotFound,
	BlockNotFound, TargetNotOwned, AlreadyUserOwned, OwnershipMetadataWriteFailed,
	NodeCommentWriteFailed, JournalWriteFailed, ReviewWriteFailed,
	RollbackBlocked, RollbackFailed, AssetStateChangedDuringConversion,
	WritePermissionDisabled, ProfilePolicyViolation, BridgeDisconnected
};

inline const TCHAR* OwnershipErrorCodeToString(EBlueprintHelperOwnershipErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperOwnershipErrorCode::InvalidRequest:                    return TEXT("invalid_request");
	case EBlueprintHelperOwnershipErrorCode::UnsupportedOwnershipScope:         return TEXT("unsupported_ownership_scope");
	case EBlueprintHelperOwnershipErrorCode::TargetBlueprintNotFound:           return TEXT("target_blueprint_not_found");
	case EBlueprintHelperOwnershipErrorCode::TargetGraphNotFound:               return TEXT("target_graph_not_found");
	case EBlueprintHelperOwnershipErrorCode::BlockNotFound:                     return TEXT("block_not_found");
	case EBlueprintHelperOwnershipErrorCode::TargetNotOwned:                    return TEXT("target_not_owned");
	case EBlueprintHelperOwnershipErrorCode::AlreadyUserOwned:                  return TEXT("already_user_owned");
	case EBlueprintHelperOwnershipErrorCode::OwnershipMetadataWriteFailed:      return TEXT("ownership_metadata_write_failed");
	case EBlueprintHelperOwnershipErrorCode::NodeCommentWriteFailed:            return TEXT("node_comment_write_failed");
	case EBlueprintHelperOwnershipErrorCode::JournalWriteFailed:                return TEXT("journal_write_failed");
	case EBlueprintHelperOwnershipErrorCode::ReviewWriteFailed:                 return TEXT("review_write_failed");
	case EBlueprintHelperOwnershipErrorCode::RollbackBlocked:                   return TEXT("rollback_blocked");
	case EBlueprintHelperOwnershipErrorCode::RollbackFailed:                     return TEXT("rollback_failed");
	case EBlueprintHelperOwnershipErrorCode::AssetStateChangedDuringConversion: return TEXT("asset_state_changed_during_conversion");
	case EBlueprintHelperOwnershipErrorCode::WritePermissionDisabled:           return TEXT("write_permission_disabled");
	case EBlueprintHelperOwnershipErrorCode::ProfilePolicyViolation:            return TEXT("profile_policy_violation");
	case EBlueprintHelperOwnershipErrorCode::BridgeDisconnected:                return TEXT("bridge_disconnected");
	default:                                                                      return TEXT("unknown");
	}
}

// ─── 结果结构（不含 write_ref） ───

struct FBlueprintHelperConvertBlockToUserOwnedResult
{
	int32 ConvertedCount = 0;
	TOptional<FString> ConversionStatus;
	TOptional<FString> AlreadyUserOwnedPolicy;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("converted_count"), ConvertedCount);
		if (ConversionStatus.IsSet()) J->SetStringField(TEXT("conversion_status"), *ConversionStatus);
		if (AlreadyUserOwnedPolicy.IsSet()) J->SetStringField(TEXT("already_user_owned_policy"), *AlreadyUserOwnedPolicy);
		return J;
	}
};

struct FBlueprintHelperConvertBlockToUserOwnedResultData
{
	FString Schema = TEXT("ConvertBlueprintHelperBlockToUserOwned.v1");
	FBlueprintHelperConvertBlockToUserOwnedResult ConversionResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("conversion_result"), ConversionResult.ToJson());
		return J;
	}
};

// ─── DryRun ───

struct FBlueprintHelperConvertBlockToUserOwnedDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperDryRunIssue> Conflicts;
	TArray<FBlueprintHelperDryRunIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperConvertBlockToUserOwnedDryRunData
{
	FString Schema = TEXT("ConvertBlueprintHelperBlockToUserOwnedDryRun.v1");
	FBlueprintHelperConvertBlockToUserOwnedDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const;
};
