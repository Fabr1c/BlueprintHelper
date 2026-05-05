// BlueprintHelper Service Layer 。MergeBlueprintGraph 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Structure/GraphWrite/BlueprintHelperAppendGraphTypes.h"

// ─── Merge 作用。───

enum class EBlueprintHelperMergeScope : uint8
{
	OwnedBlockCall,
	CustomEventCall,
	FunctionCall,
	InlineNodes,
	EventEntryLogic
};

inline const TCHAR* MergeScopeToString(EBlueprintHelperMergeScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperMergeScope::OwnedBlockCall:  return TEXT("owned_block_call");
	case EBlueprintHelperMergeScope::CustomEventCall: return TEXT("custom_event_call");
	case EBlueprintHelperMergeScope::FunctionCall:    return TEXT("function_call");
	case EBlueprintHelperMergeScope::InlineNodes:     return TEXT("inline_nodes");
	case EBlueprintHelperMergeScope::EventEntryLogic: return TEXT("event_entry_logic");
	default:                                          return TEXT("unknown");
	}
}

inline bool ParseMergeScope(const FString& Str, EBlueprintHelperMergeScope& Out)
{
	if (Str.Equals(TEXT("owned_block_call"), ESearchCase::IgnoreCase))  { Out = EBlueprintHelperMergeScope::OwnedBlockCall; return true; }
	if (Str.Equals(TEXT("custom_event_call"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperMergeScope::CustomEventCall; return true; }
	if (Str.Equals(TEXT("function_call"), ESearchCase::IgnoreCase))     { Out = EBlueprintHelperMergeScope::FunctionCall; return true; }
	if (Str.Equals(TEXT("inline_nodes"), ESearchCase::IgnoreCase))      { Out = EBlueprintHelperMergeScope::InlineNodes; return true; }
	if (Str.Equals(TEXT("event_entry_logic"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperMergeScope::EventEntryLogic; return true; }
	return false;
}

// ─── Insert 策略 ───

enum class EBlueprintHelperInsertStrategy : uint8
{
	AppendAfter,
	InsertBetween,
	BranchFork
};

inline const TCHAR* InsertStrategyToString(EBlueprintHelperInsertStrategy Strategy)
{
	switch (Strategy)
	{
	case EBlueprintHelperInsertStrategy::AppendAfter:   return TEXT("append_after");
	case EBlueprintHelperInsertStrategy::InsertBetween: return TEXT("insert_between");
	case EBlueprintHelperInsertStrategy::BranchFork:    return TEXT("branch_fork");
	default:                                            return TEXT("unknown");
	}
}

inline bool ParseInsertStrategy(const FString& Str, EBlueprintHelperInsertStrategy& Out)
{
	if (Str.Equals(TEXT("append_after"), ESearchCase::IgnoreCase))   { Out = EBlueprintHelperInsertStrategy::AppendAfter; return true; }
	if (Str.Equals(TEXT("insert_between"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperInsertStrategy::InsertBetween; return true; }
	if (Str.Equals(TEXT("branch_fork"), ESearchCase::IgnoreCase))    { Out = EBlueprintHelperInsertStrategy::BranchFork; return true; }
	return false;
}

// ─── Merge 错误。───

enum class EBlueprintHelperMergeErrorCode : uint8
{
	TargetBlueprintNotFound,
	TargetGraphNotFound,
	TargetGraphTypeInvalid,
	AnchorNodeNotFound,
	AnchorPinNotFound,
	AnchorPinNotExec,
	AnchorExecPinAlreadyConnected,
	AnchorExecPinHasMultipleSuccessors,
	OriginalSuccessorNotFound,
	InsertedLogicNotFound,
	InsertedLogicNotCallable,
	InsertedLogicHasNoExecPins,
	InsertedLogicSignatureMismatch,
	SequenceOrderRequired,
	SequenceOrderInvalid,
	UnsupportedMergeScope,
	UnsupportedInsertStrategy,
	PinTypeMismatch,
	LinkCreateFailed,
	LinkDisconnectFailed,
	SchemaRejected,
	JournalWriteFailed,
	RollbackBlocked,
	RollbackFailed,
	WritePermissionDisabled,
	ProfilePolicyViolation,
	BridgeDisconnected
};

inline const TCHAR* MergeErrorCodeToString(EBlueprintHelperMergeErrorCode Code)
{
	switch (Code)
	{
	case EBlueprintHelperMergeErrorCode::TargetBlueprintNotFound:           return TEXT("target_blueprint_not_found");
	case EBlueprintHelperMergeErrorCode::TargetGraphNotFound:               return TEXT("target_graph_not_found");
	case EBlueprintHelperMergeErrorCode::TargetGraphTypeInvalid:            return TEXT("target_graph_type_invalid");
	case EBlueprintHelperMergeErrorCode::AnchorNodeNotFound:                return TEXT("anchor_node_not_found");
	case EBlueprintHelperMergeErrorCode::AnchorPinNotFound:                 return TEXT("anchor_pin_not_found");
	case EBlueprintHelperMergeErrorCode::AnchorPinNotExec:                  return TEXT("anchor_pin_not_exec");
	case EBlueprintHelperMergeErrorCode::AnchorExecPinAlreadyConnected:      return TEXT("anchor_exec_pin_already_connected");
	case EBlueprintHelperMergeErrorCode::AnchorExecPinHasMultipleSuccessors: return TEXT("anchor_exec_pin_has_multiple_successors");
	case EBlueprintHelperMergeErrorCode::OriginalSuccessorNotFound:          return TEXT("original_successor_not_found");
	case EBlueprintHelperMergeErrorCode::InsertedLogicNotFound:             return TEXT("inserted_logic_not_found");
	case EBlueprintHelperMergeErrorCode::InsertedLogicNotCallable:          return TEXT("inserted_logic_not_callable");
	case EBlueprintHelperMergeErrorCode::InsertedLogicHasNoExecPins:        return TEXT("inserted_logic_has_no_exec_pins");
	case EBlueprintHelperMergeErrorCode::InsertedLogicSignatureMismatch:    return TEXT("inserted_logic_signature_mismatch");
	case EBlueprintHelperMergeErrorCode::SequenceOrderRequired:             return TEXT("sequence_order_required");
	case EBlueprintHelperMergeErrorCode::SequenceOrderInvalid:              return TEXT("sequence_order_invalid");
	case EBlueprintHelperMergeErrorCode::UnsupportedMergeScope:             return TEXT("unsupported_merge_scope");
	case EBlueprintHelperMergeErrorCode::UnsupportedInsertStrategy:          return TEXT("unsupported_insert_strategy");
	case EBlueprintHelperMergeErrorCode::PinTypeMismatch:                   return TEXT("pin_type_mismatch");
	case EBlueprintHelperMergeErrorCode::LinkCreateFailed:                  return TEXT("link_create_failed");
	case EBlueprintHelperMergeErrorCode::LinkDisconnectFailed:              return TEXT("link_disconnect_failed");
	case EBlueprintHelperMergeErrorCode::SchemaRejected:                    return TEXT("schema_rejected");
	case EBlueprintHelperMergeErrorCode::JournalWriteFailed:                return TEXT("journal_write_failed");
	case EBlueprintHelperMergeErrorCode::RollbackBlocked:                   return TEXT("rollback_blocked");
	case EBlueprintHelperMergeErrorCode::RollbackFailed:                    return TEXT("rollback_failed");
	case EBlueprintHelperMergeErrorCode::WritePermissionDisabled:           return TEXT("write_permission_disabled");
	case EBlueprintHelperMergeErrorCode::ProfilePolicyViolation:            return TEXT("profile_policy_violation");
	case EBlueprintHelperMergeErrorCode::BridgeDisconnected:                return TEXT("bridge_disconnected");
	default:                                                                  return TEXT("unknown");
	}
}

// ─── MergedRef ───

struct FBlueprintHelperMergedRef
{
	FString GraphId;
	FString AnchorRef;
	FString InsertedRef;
	FString SequenceRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("graph_id"), GraphId);
		Json->SetStringField(TEXT("anchor_ref"), AnchorRef);
		Json->SetStringField(TEXT("inserted_ref"), InsertedRef);
		if (!SequenceRef.IsEmpty()) Json->SetStringField(TEXT("sequence_ref"), SequenceRef);
		return Json;
	}
};

struct FBlueprintHelperMergeGraphResult
{
	FBlueprintHelperMergedRef MergedRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("merged_ref"), MergedRef.ToJson());
		return Json;
	}
};

struct FBlueprintHelperMergeGraphResultData
{
	FString Schema = TEXT("MergeBlueprintGraph.v1");
	FBlueprintHelperMergeGraphResult MergeResult;
	FBlueprintHelperWriteRef WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("merge_result"), MergeResult.ToJson());
		Json->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		return Json;
	}
};

// ─── DryRun ───

struct FBlueprintHelperMergeDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperDryRunIssue> Conflicts;
	TArray<FBlueprintHelperDryRunIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMergeDryRunData
{
	FString Schema = TEXT("MergeBlueprintGraphDryRun.v1");
	FBlueprintHelperMergeDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── Merge target ref（不输出 target_type。───

struct FBlueprintHelperMergeTargetRef
{
	FString AssetPath;
	FString Graph;
	FString MergeScope;
	FString InsertStrategy;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("graph"), Graph);
		Json->SetStringField(TEXT("merge_scope"), MergeScope);
		Json->SetStringField(TEXT("insert_strategy"), InsertStrategy);
		return Json;
	}
};
