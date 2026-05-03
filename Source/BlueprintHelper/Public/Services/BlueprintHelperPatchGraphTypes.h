// BlueprintHelper Service Layer — PatchBlueprintGraph 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Services/BlueprintHelperAppendGraphTypes.h"
#include "Services/BlueprintHelperReplaceGraphTypes.h"

// ─── Patch 作用域 ───

/** Patch 的目标作用域。 */
enum class EBlueprintHelperPatchScope : uint8
{
	PinDefault,
	NodeProperty,
	NodeComment,
	NodePosition,
	ConnectPins,
	DisconnectLink,
	ReplaceLink,
	CallTarget,
	LocalVariableRef
};

inline const TCHAR* PatchScopeToString(EBlueprintHelperPatchScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperPatchScope::PinDefault:        return TEXT("pin_default");
	case EBlueprintHelperPatchScope::NodeProperty:      return TEXT("node_property");
	case EBlueprintHelperPatchScope::NodeComment:       return TEXT("node_comment");
	case EBlueprintHelperPatchScope::NodePosition:      return TEXT("node_position");
	case EBlueprintHelperPatchScope::ConnectPins:       return TEXT("connect_pins");
	case EBlueprintHelperPatchScope::DisconnectLink:    return TEXT("disconnect_link");
	case EBlueprintHelperPatchScope::ReplaceLink:       return TEXT("replace_link");
	case EBlueprintHelperPatchScope::CallTarget:        return TEXT("call_target");
	case EBlueprintHelperPatchScope::LocalVariableRef:  return TEXT("local_variable_ref");
	default:                                            return TEXT("unknown");
	}
}

inline bool ParsePatchScope(const FString& Str, EBlueprintHelperPatchScope& Out)
{
	if (Str.Equals(TEXT("pin_default"), ESearchCase::IgnoreCase))       { Out = EBlueprintHelperPatchScope::PinDefault; return true; }
	if (Str.Equals(TEXT("node_property"), ESearchCase::IgnoreCase))     { Out = EBlueprintHelperPatchScope::NodeProperty; return true; }
	if (Str.Equals(TEXT("node_comment"), ESearchCase::IgnoreCase))      { Out = EBlueprintHelperPatchScope::NodeComment; return true; }
	if (Str.Equals(TEXT("node_position"), ESearchCase::IgnoreCase))     { Out = EBlueprintHelperPatchScope::NodePosition; return true; }
	if (Str.Equals(TEXT("connect_pins"), ESearchCase::IgnoreCase))      { Out = EBlueprintHelperPatchScope::ConnectPins; return true; }
	if (Str.Equals(TEXT("disconnect_link"), ESearchCase::IgnoreCase))   { Out = EBlueprintHelperPatchScope::DisconnectLink; return true; }
	if (Str.Equals(TEXT("replace_link"), ESearchCase::IgnoreCase))      { Out = EBlueprintHelperPatchScope::ReplaceLink; return true; }
	if (Str.Equals(TEXT("call_target"), ESearchCase::IgnoreCase))       { Out = EBlueprintHelperPatchScope::CallTarget; return true; }
	if (Str.Equals(TEXT("local_variable_ref"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperPatchScope::LocalVariableRef; return true; }
	return false;
}

// ─── Patch 类型（操作） ───

enum class EBlueprintHelperPatchType : uint8
{
	SetPinDefault,
	SetNodeProperty,
	SetNodeComment,
	SetNodePosition,
	ConnectPins,
	DisconnectLink,
	ReplaceLink,
	SetCallTarget,
	RenameLocalVariableRef
};

inline const TCHAR* PatchTypeToString(EBlueprintHelperPatchType Type)
{
	switch (Type)
	{
	case EBlueprintHelperPatchType::SetPinDefault:           return TEXT("set_pin_default");
	case EBlueprintHelperPatchType::SetNodeProperty:         return TEXT("set_node_property");
	case EBlueprintHelperPatchType::SetNodeComment:          return TEXT("set_node_comment");
	case EBlueprintHelperPatchType::SetNodePosition:         return TEXT("set_node_position");
	case EBlueprintHelperPatchType::ConnectPins:             return TEXT("connect_pins");
	case EBlueprintHelperPatchType::DisconnectLink:          return TEXT("disconnect_link");
	case EBlueprintHelperPatchType::ReplaceLink:             return TEXT("replace_link");
	case EBlueprintHelperPatchType::SetCallTarget:           return TEXT("set_call_target");
	case EBlueprintHelperPatchType::RenameLocalVariableRef:  return TEXT("rename_local_variable_ref");
	default:                                                 return TEXT("unknown");
	}
}

inline bool ParsePatchType(const FString& Str, EBlueprintHelperPatchType& Out)
{
	if (Str.Equals(TEXT("set_pin_default"), ESearchCase::IgnoreCase))            { Out = EBlueprintHelperPatchType::SetPinDefault; return true; }
	if (Str.Equals(TEXT("set_node_property"), ESearchCase::IgnoreCase))          { Out = EBlueprintHelperPatchType::SetNodeProperty; return true; }
	if (Str.Equals(TEXT("set_node_comment"), ESearchCase::IgnoreCase))           { Out = EBlueprintHelperPatchType::SetNodeComment; return true; }
	if (Str.Equals(TEXT("set_node_position"), ESearchCase::IgnoreCase))          { Out = EBlueprintHelperPatchType::SetNodePosition; return true; }
	if (Str.Equals(TEXT("connect_pins"), ESearchCase::IgnoreCase))               { Out = EBlueprintHelperPatchType::ConnectPins; return true; }
	if (Str.Equals(TEXT("disconnect_link"), ESearchCase::IgnoreCase))            { Out = EBlueprintHelperPatchType::DisconnectLink; return true; }
	if (Str.Equals(TEXT("replace_link"), ESearchCase::IgnoreCase))               { Out = EBlueprintHelperPatchType::ReplaceLink; return true; }
	if (Str.Equals(TEXT("set_call_target"), ESearchCase::IgnoreCase))            { Out = EBlueprintHelperPatchType::SetCallTarget; return true; }
	if (Str.Equals(TEXT("rename_local_variable_ref"), ESearchCase::IgnoreCase))  { Out = EBlueprintHelperPatchType::RenameLocalVariableRef; return true; }
	return false;
}

// ─── Patch 错误码 ───

enum class EBlueprintHelperPatchErrorCode : uint8
{
	InvalidPatchSchema,
	TargetBlueprintNotFound,
	TargetNotBlueprint,
	TargetGraphNotFound,
	TargetGroupNotFound,
	TargetNodeNotFound,
	TargetPinNotFound,
	TargetLinkNotFound,
	TargetAmbiguous,
	CrossGroupRefDisallowed,
	DisplayNameOnlyDisallowed,
	ExpectedOldStateMismatch,
	NodePropertyNotFound,
	NodePropertyNotWritable,
	PinDefaultNotWritable,
	PinTypeMismatch,
	LinkCreateFailed,
	LinkDisconnectFailed,
	UnsupportedPatchType,
	WritePermissionDisabled,
	ProfilePolicyViolation,
	JournalWriteFailed,
	RollbackBlocked,
	RollbackFailed,
	BridgeDisconnected
};

inline const TCHAR* PatchErrorCodeToString(EBlueprintHelperPatchErrorCode Code)
{
	switch (Code)
	{
	case EBlueprintHelperPatchErrorCode::InvalidPatchSchema:          return TEXT("invalid_patch_schema");
	case EBlueprintHelperPatchErrorCode::TargetBlueprintNotFound:     return TEXT("target_blueprint_not_found");
	case EBlueprintHelperPatchErrorCode::TargetNotBlueprint:          return TEXT("target_not_blueprint");
	case EBlueprintHelperPatchErrorCode::TargetGraphNotFound:         return TEXT("target_graph_not_found");
	case EBlueprintHelperPatchErrorCode::TargetGroupNotFound:         return TEXT("target_group_not_found");
	case EBlueprintHelperPatchErrorCode::TargetNodeNotFound:          return TEXT("target_node_not_found");
	case EBlueprintHelperPatchErrorCode::TargetPinNotFound:           return TEXT("target_pin_not_found");
	case EBlueprintHelperPatchErrorCode::TargetLinkNotFound:          return TEXT("target_link_not_found");
	case EBlueprintHelperPatchErrorCode::TargetAmbiguous:             return TEXT("target_ambiguous");
	case EBlueprintHelperPatchErrorCode::CrossGroupRefDisallowed:     return TEXT("cross_group_ref_disallowed");
	case EBlueprintHelperPatchErrorCode::DisplayNameOnlyDisallowed:   return TEXT("display_name_only_disallowed");
	case EBlueprintHelperPatchErrorCode::ExpectedOldStateMismatch:    return TEXT("expected_old_state_mismatch");
	case EBlueprintHelperPatchErrorCode::NodePropertyNotFound:        return TEXT("node_property_not_found");
	case EBlueprintHelperPatchErrorCode::NodePropertyNotWritable:     return TEXT("node_property_not_writable");
	case EBlueprintHelperPatchErrorCode::PinDefaultNotWritable:       return TEXT("pin_default_not_writable");
	case EBlueprintHelperPatchErrorCode::PinTypeMismatch:             return TEXT("pin_type_mismatch");
	case EBlueprintHelperPatchErrorCode::LinkCreateFailed:            return TEXT("link_create_failed");
	case EBlueprintHelperPatchErrorCode::LinkDisconnectFailed:        return TEXT("link_disconnect_failed");
	case EBlueprintHelperPatchErrorCode::UnsupportedPatchType:        return TEXT("unsupported_patch_type");
	case EBlueprintHelperPatchErrorCode::WritePermissionDisabled:     return TEXT("write_permission_disabled");
	case EBlueprintHelperPatchErrorCode::ProfilePolicyViolation:      return TEXT("profile_policy_violation");
	case EBlueprintHelperPatchErrorCode::JournalWriteFailed:          return TEXT("journal_write_failed");
	case EBlueprintHelperPatchErrorCode::RollbackBlocked:             return TEXT("rollback_blocked");
	case EBlueprintHelperPatchErrorCode::RollbackFailed:              return TEXT("rollback_failed");
	case EBlueprintHelperPatchErrorCode::BridgeDisconnected:          return TEXT("bridge_disconnected");
	default:                                                           return TEXT("unknown");
	}
}

// ─── Patch 结果结构 ───

struct FBlueprintHelperPatchedRef
{
	FString GraphId;
	TOptional<FString> NodeRef;
	TOptional<FString> PinRef;
	TOptional<FString> LinkRef;
	TOptional<FString> NodePath;
	TOptional<FString> PinPath;
	TOptional<FString> LinkPath;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("graph_id"), GraphId);
		if (NodeRef.IsSet()) Json->SetStringField(TEXT("node_ref"), *NodeRef);
		if (PinRef.IsSet()) Json->SetStringField(TEXT("pin_ref"), *PinRef);
		if (LinkRef.IsSet()) Json->SetStringField(TEXT("link_ref"), *LinkRef);
		if (NodePath.IsSet()) Json->SetStringField(TEXT("node_path"), *NodePath);
		if (PinPath.IsSet()) Json->SetStringField(TEXT("pin_path"), *PinPath);
		if (LinkPath.IsSet()) Json->SetStringField(TEXT("link_path"), *LinkPath);
		return Json;
	}
};

struct FBlueprintHelperPatchSummary
{
	FString PatchType;
	bool bExpectedOldStateProvided = false;
	bool bChanged = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("patch_type"), PatchType);
		Json->SetBoolField(TEXT("expected_old_state_provided"), bExpectedOldStateProvided);
		Json->SetBoolField(TEXT("changed"), bChanged);
		return Json;
	}
};

struct FBlueprintHelperPatchGraphResult
{
	FBlueprintHelperPatchedRef PatchedRef;
	FBlueprintHelperPatchSummary Patch;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("patched_ref"), PatchedRef.ToJson());
		Json->SetObjectField(TEXT("patch"), Patch.ToJson());
		return Json;
	}
};

struct FBlueprintHelperPatchGraphResultData
{
	FString Schema = TEXT("PatchBlueprintGraph.v1");
	FBlueprintHelperPatchGraphResult PatchResult;
	FBlueprintHelperWriteRef WriteRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("patch_result"), PatchResult.ToJson());
		Json->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		return Json;
	}
};

// ─── DryRun 结构 ───

struct FBlueprintHelperPatchDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
	TArray<FBlueprintHelperGraphWriteIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPatchDryRunData
{
	FString Schema = TEXT("PatchBlueprintGraphDryRun.v1");
	FBlueprintHelperPatchDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const;
};
