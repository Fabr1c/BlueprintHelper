// BlueprintHelper Service Layer — AppendBlueprintGraph 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── Append 阶段枚举 ───

/** AppendBlueprintGraph 执行阶段。 */
enum class EBlueprintHelperAppendStage : uint8
{
	ParseInput,
	Auth,
	ResolveTarget,
	Preflight,
	CreateGraph,
	CreateNodes,
	ConnectPins,
	WriteMetadata,
	WriteJournal,
	Rollback
};

/** AppendStage → MCP snake_case string。 */
inline const TCHAR* AppendStageToString(EBlueprintHelperAppendStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperAppendStage::ParseInput:    return TEXT("parse_input");
	case EBlueprintHelperAppendStage::Auth:          return TEXT("auth");
	case EBlueprintHelperAppendStage::ResolveTarget: return TEXT("resolve_target");
	case EBlueprintHelperAppendStage::Preflight:     return TEXT("preflight");
	case EBlueprintHelperAppendStage::CreateGraph:   return TEXT("create_graph");
	case EBlueprintHelperAppendStage::CreateNodes:   return TEXT("create_nodes");
	case EBlueprintHelperAppendStage::ConnectPins:   return TEXT("connect_pins");
	case EBlueprintHelperAppendStage::WriteMetadata: return TEXT("write_metadata");
	case EBlueprintHelperAppendStage::WriteJournal:  return TEXT("write_journal");
	case EBlueprintHelperAppendStage::Rollback:      return TEXT("rollback");
	default:                                         return TEXT("unknown");
	}
}

// ─── Append 错误码枚举 ───

/** AppendBlueprintGraph 专属错误码。 */
enum class EBlueprintHelperAppendErrorCode : uint8
{
	TargetBlueprintNotFound,
	TargetNotBlueprint,
	TargetGraphNotEmpty,
	TargetGraphTypeInvalid,
	CustomEventAlreadyExists,
	GlobalEventCreationDisallowed,
	FunctionNotFound,
	EventNotFound,
	CallSignatureMismatch,
	PinNotFound,
	PinTypeMismatch,
	SchemaRejected,
	NodeCreateFailed,
	LinkCreateFailed,
	OwnershipWriteFailed,
	JournalWriteFailed,
	RollbackBlocked,
	RollbackFailed,
	WritePermissionDisabled,
	ProfilePolicyViolation,
	BridgeDisconnected
};

/** AppendErrorCode → MCP snake_case string。 */
inline const TCHAR* AppendErrorCodeToString(EBlueprintHelperAppendErrorCode Code)
{
	switch (Code)
	{
	case EBlueprintHelperAppendErrorCode::TargetBlueprintNotFound:       return TEXT("target_blueprint_not_found");
	case EBlueprintHelperAppendErrorCode::TargetNotBlueprint:            return TEXT("target_not_blueprint");
	case EBlueprintHelperAppendErrorCode::TargetGraphNotEmpty:           return TEXT("target_graph_not_empty");
	case EBlueprintHelperAppendErrorCode::TargetGraphTypeInvalid:        return TEXT("target_graph_type_invalid");
	case EBlueprintHelperAppendErrorCode::CustomEventAlreadyExists:       return TEXT("custom_event_already_exists");
	case EBlueprintHelperAppendErrorCode::GlobalEventCreationDisallowed:  return TEXT("global_event_creation_disallowed");
	case EBlueprintHelperAppendErrorCode::FunctionNotFound:              return TEXT("function_not_found");
	case EBlueprintHelperAppendErrorCode::EventNotFound:                 return TEXT("event_not_found");
	case EBlueprintHelperAppendErrorCode::CallSignatureMismatch:         return TEXT("call_signature_mismatch");
	case EBlueprintHelperAppendErrorCode::PinNotFound:                   return TEXT("pin_not_found");
	case EBlueprintHelperAppendErrorCode::PinTypeMismatch:               return TEXT("pin_type_mismatch");
	case EBlueprintHelperAppendErrorCode::SchemaRejected:                return TEXT("schema_rejected");
	case EBlueprintHelperAppendErrorCode::NodeCreateFailed:              return TEXT("node_create_failed");
	case EBlueprintHelperAppendErrorCode::LinkCreateFailed:              return TEXT("link_create_failed");
	case EBlueprintHelperAppendErrorCode::OwnershipWriteFailed:          return TEXT("ownership_write_failed");
	case EBlueprintHelperAppendErrorCode::JournalWriteFailed:            return TEXT("journal_write_failed");
	case EBlueprintHelperAppendErrorCode::RollbackBlocked:               return TEXT("rollback_blocked");
	case EBlueprintHelperAppendErrorCode::RollbackFailed:                return TEXT("rollback_failed");
	case EBlueprintHelperAppendErrorCode::WritePermissionDisabled:       return TEXT("write_permission_disabled");
	case EBlueprintHelperAppendErrorCode::ProfilePolicyViolation:        return TEXT("profile_policy_violation");
	case EBlueprintHelperAppendErrorCode::BridgeDisconnected:            return TEXT("bridge_disconnected");
	default:                                                              return TEXT("unknown");
	}
}

// ─── DryRun 结果枚举 ───

/** dry_run 结果。 */
enum class EBlueprintHelperDryRunResult : uint8
{
	Passed,
	Blocked
};

/** DryRunResult → MCP snake_case string。 */
inline const TCHAR* DryRunResultToString(EBlueprintHelperDryRunResult Result)
{
	switch (Result)
	{
	case EBlueprintHelperDryRunResult::Passed:  return TEXT("passed");
	case EBlueprintHelperDryRunResult::Blocked: return TEXT("blocked");
	default:                                    return TEXT("unknown");
	}
}

#pragma region Append Result Structs

// ─── Graph Info ───

/** 图表基本信息。 */
struct FBlueprintHelperAppendGraphInfo
{
	FString GraphId;
	FString GraphName;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("graph_id"), GraphId);
		Json->SetStringField(TEXT("graph_name"), GraphName);
		return Json;
	}
};

// ─── WriteRef ───

/** 写入引用。 */
struct FBlueprintHelperWriteRef
{
	FString TransactionId;
	bool bJournalRecorded = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("transaction_id"), TransactionId);
		Json->SetBoolField(TEXT("journal_recorded"), bJournalRecorded);
		return Json;
	}
};

// ─── AppendResult ───

/** Append 操作成功结果（Agent-facing）。 */
struct FBlueprintHelperAppendGraphResult
{
	FBlueprintHelperAppendGraphInfo Graph;
	TArray<FString> BlockRefs;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("graph"), Graph.ToJson());
		if (BlockRefs.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Ref : BlockRefs) { Arr.Add(MakeShared<FJsonValueString>(Ref)); }
			Json->SetArrayField(TEXT("block_refs"), Arr);
		}
		return Json;
	}
};

// ─── AppendResultData（完整 data payload） ───

/** Append 成功时的完整 data 负载。 */
struct FBlueprintHelperAppendGraphResultData
{
	FString Schema = TEXT("AppendBlueprintGraph.v1");
	FBlueprintHelperAppendGraphResult AppendResult;
	FBlueprintHelperWriteRef WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("append_result"), AppendResult.ToJson());
		Json->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		return Json;
	}
};

// ─── DryRunIssue ───

/** dry_run 阻断问题。 */
struct FBlueprintHelperDryRunIssue
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

// ─── AppendDryRunResult ───

/** Append dry_run 结果。 */
struct FBlueprintHelperAppendDryRunResult
{
	EBlueprintHelperDryRunResult Result = EBlueprintHelperDryRunResult::Passed;
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperDryRunIssue> Conflicts;
	TArray<FBlueprintHelperDryRunIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("result"), DryRunResultToString(Result));
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

// ─── AppendDryRunData ───

/** Append dry_run 完整 data 负载。 */
struct FBlueprintHelperAppendDryRunData
{
	FString Schema = TEXT("AppendBlueprintGraphDryRun.v1");
	FBlueprintHelperAppendDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
		return Json;
	}
};

// ─── AppendJournalRecord ───

/** Transaction Journal 记录。 */
struct FBlueprintHelperGraphReviewNodeAnchor
{
	FString NodePath;
	FString NodeGuid;
	FString DisplayLabel;
	FVector2D GraphPosition = FVector2D::ZeroVector;
	FVector2D GraphSize = FVector2D(360.0f, 180.0f);
	bool bHasGraphBounds = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!NodePath.IsEmpty()) Json->SetStringField(TEXT("node_path"), NodePath);
		if (!NodeGuid.IsEmpty()) Json->SetStringField(TEXT("node_guid"), NodeGuid);
		if (!DisplayLabel.IsEmpty()) Json->SetStringField(TEXT("display_label"), DisplayLabel);

		TSharedRef<FJsonObject> PositionJson = MakeShared<FJsonObject>();
		PositionJson->SetNumberField(TEXT("x"), GraphPosition.X);
		PositionJson->SetNumberField(TEXT("y"), GraphPosition.Y);
		Json->SetObjectField(TEXT("graph_position"), PositionJson);

		TSharedRef<FJsonObject> SizeJson = MakeShared<FJsonObject>();
		SizeJson->SetNumberField(TEXT("x"), GraphSize.X);
		SizeJson->SetNumberField(TEXT("y"), GraphSize.Y);
		Json->SetObjectField(TEXT("graph_size"), SizeJson);

		Json->SetBoolField(TEXT("has_graph_bounds"), bHasGraphBounds);
		return Json;
	}
};

struct FBlueprintHelperAppendJournalRecord
{
	FString TransactionId;
	FString ArchiveSessionId;
	FString TaskRunId;
	FString Tool = TEXT("AppendBlueprintGraph");
	FString Status;
	TArray<FString> TargetAssets;
	FString GraphId;
	FString GraphName;
	TArray<FString> BlockIds;
	TArray<FString> CreatedNodePaths;
	TArray<FBlueprintHelperGraphReviewNodeAnchor> CreatedNodeAnchors;
	TArray<FString> CreatedLinkPaths;
	FString DiffSummary;
	FString RollbackData;
	TArray<FString> Validation;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("transaction_id"), TransactionId);
		if (!ArchiveSessionId.IsEmpty()) Json->SetStringField(TEXT("archive_session_id"), ArchiveSessionId);
		if (!TaskRunId.IsEmpty()) Json->SetStringField(TEXT("task_run_id"), TaskRunId);
		Json->SetStringField(TEXT("tool"), Tool);
		if (!Status.IsEmpty()) Json->SetStringField(TEXT("status"), Status);
		if (TargetAssets.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Asset : TargetAssets) { Arr.Add(MakeShared<FJsonValueString>(Asset)); }
			Json->SetArrayField(TEXT("target_assets"), Arr);
		}
		if (!GraphId.IsEmpty()) Json->SetStringField(TEXT("graph"), GraphId);
		if (BlockIds.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Id : BlockIds) { Arr.Add(MakeShared<FJsonValueString>(Id)); }
			Json->SetArrayField(TEXT("blocks"), Arr);
		}
		if (CreatedNodePaths.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Node : CreatedNodePaths) { Arr.Add(MakeShared<FJsonValueString>(Node)); }
			Json->SetArrayField(TEXT("created_nodes"), Arr);
		}
		if (CreatedNodeAnchors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FBlueprintHelperGraphReviewNodeAnchor& Anchor : CreatedNodeAnchors)
			{
				Arr.Add(MakeShared<FJsonValueObject>(Anchor.ToJson()));
			}
			Json->SetArrayField(TEXT("created_node_anchors"), Arr);
		}
		if (CreatedLinkPaths.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Link : CreatedLinkPaths) { Arr.Add(MakeShared<FJsonValueString>(Link)); }
			Json->SetArrayField(TEXT("created_links"), Arr);
		}
		if (!DiffSummary.IsEmpty()) Json->SetStringField(TEXT("diff_summary"), DiffSummary);
		if (!RollbackData.IsEmpty()) Json->SetStringField(TEXT("rollback_data"), RollbackData);
		if (Validation.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& V : Validation) { Arr.Add(MakeShared<FJsonValueString>(V)); }
			Json->SetArrayField(TEXT("validation"), Arr);
		}
		return Json;
	}
};

#pragma endregion Append Result Structs
