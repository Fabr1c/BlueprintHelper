// BlueprintHelper Service Layer — ReplaceBlueprintGraph 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Services/BlueprintHelperAppendGraphTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ─── Replace 作用域枚举 ───

/** ReplaceBlueprintGraph 的替换作用域。 */
enum class EBlueprintHelperReplaceScope : uint8
{
	BlockImplementation,
	FunctionBody,
	EventBody,
	CustomEventBody,
	FunctionDefinition,
	EventDefinition,
	Graph
};

/** ReplaceScope → MCP snake_case string。 */
inline const TCHAR* ReplaceScopeToString(EBlueprintHelperReplaceScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperReplaceScope::BlockImplementation: return TEXT("block_implementation");
	case EBlueprintHelperReplaceScope::FunctionBody:        return TEXT("function_body");
	case EBlueprintHelperReplaceScope::EventBody:           return TEXT("event_body");
	case EBlueprintHelperReplaceScope::CustomEventBody:     return TEXT("custom_event_body");
	case EBlueprintHelperReplaceScope::FunctionDefinition:  return TEXT("function_definition");
	case EBlueprintHelperReplaceScope::EventDefinition:     return TEXT("event_definition");
	case EBlueprintHelperReplaceScope::Graph:               return TEXT("graph");
	default:                                                return TEXT("unknown");
	}
}

/** 从字符串解析 ReplaceScope。 */
inline bool ParseReplaceScope(const FString& String, EBlueprintHelperReplaceScope& OutScope)
{
	if (String.Equals(TEXT("block_implementation"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::BlockImplementation;
		return true;
	}
	if (String.Equals(TEXT("function_body"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::FunctionBody;
		return true;
	}
	if (String.Equals(TEXT("event_body"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::EventBody;
		return true;
	}
	if (String.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::CustomEventBody;
		return true;
	}
	if (String.Equals(TEXT("function_definition"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::FunctionDefinition;
		return true;
	}
	if (String.Equals(TEXT("event_definition"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::EventDefinition;
		return true;
	}
	if (String.Equals(TEXT("graph"), ESearchCase::IgnoreCase))
	{
		OutScope = EBlueprintHelperReplaceScope::Graph;
		return true;
	}
	return false;
}

// ─── 图写阶段枚举（Append / Replace / Merge 共用） ───

/** Graph Write 工具共用阶段。 */
enum class EBlueprintHelperGraphWriteStage : uint8
{
	ParseInput,
	Auth,
	ResolveTarget,
	Preflight,
	DryRun,
	SnapshotBefore,
	DeleteOldImplementation,
	CreateNodes,
	CreateLinks,
	PreserveEntry,
	UpdateDefinition,
	WriteMetadata,
	WriteJournal,
	Rollback
};

/** GraphWriteStage → MCP snake_case string。 */
inline const TCHAR* GraphWriteStageToString(EBlueprintHelperGraphWriteStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperGraphWriteStage::ParseInput:              return TEXT("parse_input");
	case EBlueprintHelperGraphWriteStage::Auth:                    return TEXT("auth");
	case EBlueprintHelperGraphWriteStage::ResolveTarget:           return TEXT("resolve_target");
	case EBlueprintHelperGraphWriteStage::Preflight:               return TEXT("preflight");
	case EBlueprintHelperGraphWriteStage::DryRun:                  return TEXT("dry_run");
	case EBlueprintHelperGraphWriteStage::SnapshotBefore:          return TEXT("snapshot_before");
	case EBlueprintHelperGraphWriteStage::DeleteOldImplementation:  return TEXT("delete_old_implementation");
	case EBlueprintHelperGraphWriteStage::CreateNodes:             return TEXT("create_nodes");
	case EBlueprintHelperGraphWriteStage::CreateLinks:             return TEXT("create_links");
	case EBlueprintHelperGraphWriteStage::PreserveEntry:           return TEXT("preserve_entry");
	case EBlueprintHelperGraphWriteStage::UpdateDefinition:        return TEXT("update_definition");
	case EBlueprintHelperGraphWriteStage::WriteMetadata:           return TEXT("write_metadata");
	case EBlueprintHelperGraphWriteStage::WriteJournal:            return TEXT("write_journal");
	case EBlueprintHelperGraphWriteStage::Rollback:                return TEXT("rollback");
	default:                                                        return TEXT("unknown");
	}
}

// ─── Replace 错误码 ───

/** ReplaceBlueprintGraph 专属错误码。 */
enum class EBlueprintHelperReplaceErrorCode : uint8
{
	TargetBlueprintNotFound,
	TargetNotBlueprint,
	TargetGraphNotFound,
	TargetBlockNotFound,
	TargetFunctionNotFound,
	TargetEventNotFound,
	TargetCustomEventNotFound,
	TargetAmbiguous,
	TargetNotOwned,
	ReplaceScopeUnsupported,
	SignatureChangeDisallowed,
	EntryIdentityChangeDisallowed,
	ExternalDependentsMayBreak,
	UserNodeModificationNotAllowed,
	SchemaRejected,
	NodeCreateFailed,
	LinkCreateFailed,
	PinNotFound,
	PinTypeMismatch,
	OwnershipWriteFailed,
	JournalWriteFailed,
	RollbackBlocked,
	RollbackFailed,
	WritePermissionDisabled,
	ProfilePolicyViolation,
	BridgeDisconnected
};

/** ReplaceErrorCode → MCP snake_case string。 */
inline const TCHAR* ReplaceErrorCodeToString(EBlueprintHelperReplaceErrorCode Code)
{
	switch (Code)
	{
	case EBlueprintHelperReplaceErrorCode::TargetBlueprintNotFound:       return TEXT("target_blueprint_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetNotBlueprint:            return TEXT("target_not_blueprint");
	case EBlueprintHelperReplaceErrorCode::TargetGraphNotFound:           return TEXT("target_graph_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetBlockNotFound:           return TEXT("target_block_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetFunctionNotFound:        return TEXT("target_function_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetEventNotFound:           return TEXT("target_event_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetCustomEventNotFound:     return TEXT("target_custom_event_not_found");
	case EBlueprintHelperReplaceErrorCode::TargetAmbiguous:               return TEXT("target_ambiguous");
	case EBlueprintHelperReplaceErrorCode::TargetNotOwned:                return TEXT("target_not_owned");
	case EBlueprintHelperReplaceErrorCode::ReplaceScopeUnsupported:       return TEXT("replace_scope_unsupported");
	case EBlueprintHelperReplaceErrorCode::SignatureChangeDisallowed:     return TEXT("signature_change_disallowed");
	case EBlueprintHelperReplaceErrorCode::EntryIdentityChangeDisallowed: return TEXT("entry_identity_change_disallowed");
	case EBlueprintHelperReplaceErrorCode::ExternalDependentsMayBreak:    return TEXT("external_dependents_may_break");
	case EBlueprintHelperReplaceErrorCode::UserNodeModificationNotAllowed: return TEXT("user_node_modification_not_allowed");
	case EBlueprintHelperReplaceErrorCode::SchemaRejected:                return TEXT("schema_rejected");
	case EBlueprintHelperReplaceErrorCode::NodeCreateFailed:              return TEXT("node_create_failed");
	case EBlueprintHelperReplaceErrorCode::LinkCreateFailed:              return TEXT("link_create_failed");
	case EBlueprintHelperReplaceErrorCode::PinNotFound:                   return TEXT("pin_not_found");
	case EBlueprintHelperReplaceErrorCode::PinTypeMismatch:               return TEXT("pin_type_mismatch");
	case EBlueprintHelperReplaceErrorCode::OwnershipWriteFailed:          return TEXT("ownership_write_failed");
	case EBlueprintHelperReplaceErrorCode::JournalWriteFailed:            return TEXT("journal_write_failed");
	case EBlueprintHelperReplaceErrorCode::RollbackBlocked:               return TEXT("rollback_blocked");
	case EBlueprintHelperReplaceErrorCode::RollbackFailed:                return TEXT("rollback_failed");
	case EBlueprintHelperReplaceErrorCode::WritePermissionDisabled:       return TEXT("write_permission_disabled");
	case EBlueprintHelperReplaceErrorCode::ProfilePolicyViolation:        return TEXT("profile_policy_violation");
	case EBlueprintHelperReplaceErrorCode::BridgeDisconnected:            return TEXT("bridge_disconnected");
	default:                                                               return TEXT("unknown");
	}
}

// ─── 图写通用问题 ───

/** Graph Write 通用 issue（用于 dry_run conflicts/errors）。 */
struct FBlueprintHelperGraphWriteIssue
{
	FString Code;
	FString Message;
	FString Target;
	FString Source;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
		if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
		if (!Target.IsEmpty()) Json->SetStringField(TEXT("target"), Target);
		if (!Source.IsEmpty()) Json->SetStringField(TEXT("source"), Source);
		return Json;
	}
};

// ─── ReplacedRef ───

/** 被替换目标的引用。 */
struct FBlueprintHelperReplacedRef
{
	FString GraphId;
	FString TargetRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("graph_id"), GraphId);
		Json->SetStringField(TEXT("target_ref"), TargetRef);
		return Json;
	}
};

// ─── ReplaceGraphResult ───

/** Replace 操作成功结果（Agent-facing）。 */
struct FBlueprintHelperReplaceGraphResult
{
	FBlueprintHelperReplacedRef ReplacedRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("replaced_ref"), ReplacedRef.ToJson());
		return Json;
	}
};

// ─── ReplaceGraphResultData ───

/** Replace 成功时完整 data 负载。 */
struct FBlueprintHelperReplaceGraphResultData
{
	FString Schema = TEXT("ReplaceBlueprintGraph.v1");
	FBlueprintHelperReplaceGraphResult ReplaceResult;
	FBlueprintHelperWriteRef WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("replace_result"), ReplaceResult.ToJson());
		Json->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		return Json;
	}
};

// ─── ReplaceDryRunResult ───

/** Replace dry_run 结果。 */
struct FBlueprintHelperReplaceDryRunResult
{
	FString Result = TEXT("passed"); // passed | blocked
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
	TArray<FBlueprintHelperGraphWriteIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("result"), Result);
		Json->SetBoolField(TEXT("can_execute"), bCanExecute);
		if (BlockedBy.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Item : BlockedBy) { Arr.Add(MakeShared<FJsonValueString>(Item)); }
			Json->SetArrayField(TEXT("blocked_by"), Arr);
		}
		if (Conflicts.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& Issue : Conflicts) { Arr.Add(MakeShared<FJsonValueObject>(Issue.ToJson())); }
			Json->SetArrayField(TEXT("conflicts"), Arr);
		}
		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& Issue : Errors) { Arr.Add(MakeShared<FJsonValueObject>(Issue.ToJson())); }
			Json->SetArrayField(TEXT("errors"), Arr);
		}
		return Json;
	}
};

// ─── ReplaceDryRunData ───

/** Replace dry_run 完整 data 负载。 */
struct FBlueprintHelperReplaceDryRunData
{
	FString Schema = TEXT("ReplaceBlueprintGraphDryRun.v1");
	FBlueprintHelperReplaceDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
		return Json;
	}
};

// ─── Replace Journal Record ───

/** Replace Transaction Journal 记录。 */
struct FBlueprintHelperReplaceJournalRecord
{
	FString Schema = TEXT("BlueprintHelper.TransactionJournal.v1");
	FString TransactionId;
	FString Tool = TEXT("ReplaceBlueprintGraph");
	FString Status;
	TArray<FString> TargetAssets;
	FString ReplaceScopeStr;
	FString AssetPath;
	FString GraphName;
	FString TargetRef;
	FString BlockId;
	FString BeforeSnapshotJson;
	FString AfterSnapshotSummaryJson;
	FString RollbackDataJson;
	bool bShouldCompile = true;
	bool bShouldSave = true;
	bool bCompiled = false;
	bool bSaved = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetStringField(TEXT("transaction_id"), TransactionId);
		Json->SetStringField(TEXT("tool"), Tool);
		if (!Status.IsEmpty()) Json->SetStringField(TEXT("status"), Status);
		if (TargetAssets.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Asset : TargetAssets) { Arr.Add(MakeShared<FJsonValueString>(Asset)); }
			Json->SetArrayField(TEXT("target_assets"), Arr);
		}
		Json->SetStringField(TEXT("replace_scope"), ReplaceScopeStr);

		TSharedRef<FJsonObject> TargetObj = MakeShared<FJsonObject>();
		TargetObj->SetStringField(TEXT("asset_path"), AssetPath);
		TargetObj->SetStringField(TEXT("graph"), GraphName);
		TargetObj->SetStringField(TEXT("target_ref"), TargetRef);
		if (!BlockId.IsEmpty()) TargetObj->SetStringField(TEXT("block_id"), BlockId);
		Json->SetObjectField(TEXT("target"), TargetObj);

		if (!BeforeSnapshotJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> SnapshotParsed;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BeforeSnapshotJson);
			if (FJsonSerializer::Deserialize(Reader, SnapshotParsed) && SnapshotParsed.IsValid())
			{
				Json->SetObjectField(TEXT("before_snapshot"), SnapshotParsed);
			}
		}
		if (!AfterSnapshotSummaryJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> AfterParsed;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AfterSnapshotSummaryJson);
			if (FJsonSerializer::Deserialize(Reader, AfterParsed) && AfterParsed.IsValid())
			{
				Json->SetObjectField(TEXT("after_snapshot_summary"), AfterParsed);
			}
		}
		if (!RollbackDataJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> RollbackParsed;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RollbackDataJson);
			if (FJsonSerializer::Deserialize(Reader, RollbackParsed) && RollbackParsed.IsValid())
			{
				Json->SetObjectField(TEXT("rollback_data"), RollbackParsed);
			}
		}

		TSharedRef<FJsonObject> ValidationObj = MakeShared<FJsonObject>();
		ValidationObj->SetBoolField(TEXT("should_compile"), bShouldCompile);
		ValidationObj->SetBoolField(TEXT("should_save"), bShouldSave);
		ValidationObj->SetBoolField(TEXT("compiled"), bCompiled);
		ValidationObj->SetBoolField(TEXT("saved"), bSaved);
		Json->SetObjectField(TEXT("validation"), ValidationObj);

		return Json;
	}
};
