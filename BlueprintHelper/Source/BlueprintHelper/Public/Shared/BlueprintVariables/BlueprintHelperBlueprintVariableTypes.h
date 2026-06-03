// BlueprintHelper Service Layer — Blueprint Variable / Default / Local Variable 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperVariableScope : uint8 { Member, Local };
inline const TCHAR* VariableScopeToString(EBlueprintHelperVariableScope S)
{
	switch (S) { case EBlueprintHelperVariableScope::Member: return TEXT("member"); case EBlueprintHelperVariableScope::Local: return TEXT("local"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperVariableReadScope : uint8 { MemberVariables, MemberDefaults, LocalVariables };
inline const TCHAR* VariableReadScopeToString(EBlueprintHelperVariableReadScope S)
{
	switch (S) { case EBlueprintHelperVariableReadScope::MemberVariables: return TEXT("member_variables"); case EBlueprintHelperVariableReadScope::MemberDefaults: return TEXT("member_defaults"); case EBlueprintHelperVariableReadScope::LocalVariables: return TEXT("local_variables"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperVariableNameCollisionPolicy : uint8 { FailIfExists, ReuseIfExists };
inline const TCHAR* VariableNameCollisionPolicyToString(EBlueprintHelperVariableNameCollisionPolicy P)
{
	switch (P) { case EBlueprintHelperVariableNameCollisionPolicy::FailIfExists: return TEXT("fail_if_exists"); case EBlueprintHelperVariableNameCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperVariableErrorCode : uint8
{
	InvalidRequest, AssetNotFound, TargetNotBlueprint, FunctionNotFound,
	VariableNotFound, VariableAlreadyExists, UnsupportedVariableType,
	UnsupportedVariableContainer, VariableTypeChangeUnsupported, VariableRenameUnsupported,
	InvalidMemberVariableSettings, InvalidMemberDefaultSettings, InvalidLocalVariableSettings,
	VariableReferencesExist, RemoveVariableDryRunRequired, TypeMismatch,
	VariableAddFailed, VariablePropertySetFailed, VariableDefaultSetFailed,
	VariableRemoveFailed, RollbackFailed, InternalError
};
inline const TCHAR* VariableErrorCodeToString(EBlueprintHelperVariableErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperVariableErrorCode::InvalidRequest:                   return TEXT("invalid_request");
	case EBlueprintHelperVariableErrorCode::AssetNotFound:                    return TEXT("asset_not_found");
	case EBlueprintHelperVariableErrorCode::TargetNotBlueprint:               return TEXT("target_not_blueprint");
	case EBlueprintHelperVariableErrorCode::FunctionNotFound:                 return TEXT("function_not_found");
	case EBlueprintHelperVariableErrorCode::VariableNotFound:                 return TEXT("variable_not_found");
	case EBlueprintHelperVariableErrorCode::VariableAlreadyExists:            return TEXT("variable_already_exists");
	case EBlueprintHelperVariableErrorCode::UnsupportedVariableType:          return TEXT("unsupported_variable_type");
	case EBlueprintHelperVariableErrorCode::UnsupportedVariableContainer:     return TEXT("unsupported_variable_container");
	case EBlueprintHelperVariableErrorCode::VariableTypeChangeUnsupported:    return TEXT("variable_type_change_unsupported");
	case EBlueprintHelperVariableErrorCode::VariableRenameUnsupported:        return TEXT("variable_rename_unsupported");
	case EBlueprintHelperVariableErrorCode::InvalidMemberVariableSettings:    return TEXT("invalid_member_variable_settings");
	case EBlueprintHelperVariableErrorCode::InvalidMemberDefaultSettings:     return TEXT("invalid_member_default_settings");
	case EBlueprintHelperVariableErrorCode::InvalidLocalVariableSettings:     return TEXT("invalid_local_variable_settings");
	case EBlueprintHelperVariableErrorCode::VariableReferencesExist:          return TEXT("variable_references_exist");
	case EBlueprintHelperVariableErrorCode::RemoveVariableDryRunRequired:     return TEXT("remove_variable_dry_run_required");
	case EBlueprintHelperVariableErrorCode::TypeMismatch:                     return TEXT("type_mismatch");
	case EBlueprintHelperVariableErrorCode::VariableAddFailed:                return TEXT("variable_add_failed");
	case EBlueprintHelperVariableErrorCode::VariablePropertySetFailed:        return TEXT("variable_property_set_failed");
	case EBlueprintHelperVariableErrorCode::VariableDefaultSetFailed:         return TEXT("variable_default_set_failed");
	case EBlueprintHelperVariableErrorCode::VariableRemoveFailed:             return TEXT("variable_remove_failed");
	case EBlueprintHelperVariableErrorCode::RollbackFailed:                   return TEXT("rollback_failed");
	case EBlueprintHelperVariableErrorCode::InternalError:                    return TEXT("internal_error");
	default:                                                                     return TEXT("unknown");
	}
}

// ─── VariableType ───

struct FBlueprintHelperVariableType
{
	FString Category; // bool | int | float | object | class | struct | enum | map
	TOptional<FString> Subtype;
	FString Container = TEXT("single"); // single | array | set | map
	TOptional<TSharedPtr<FBlueprintHelperVariableType>> KeyType;
	TOptional<TSharedPtr<FBlueprintHelperVariableType>> ValueType;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperVariableReplicationFacts
{
	FString Mode = TEXT("none");
	FString Condition = TEXT("none");
	FString ConditionEngineName = TEXT("COND_None");
	FString NotifyFunctionName;
	bool bNotifyGraphExists = false;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── MemberVariable 摘要 ───

struct FBlueprintHelperMemberVariableItem
{
	FString VariableName;
	FBlueprintHelperVariableType VariableType;
	TOptional<FString> Category;
	TOptional<FString> Tooltip;
	bool bInstanceEditable = false;
	bool bExposeOnSpawn = false;
	FBlueprintHelperVariableReplicationFacts Replication;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── ReadMemberVariables ───

struct FBlueprintHelperReadMemberVariablesResultData
{
	FString Schema = TEXT("ReadMemberVariables.v1");
	TArray<FBlueprintHelperMemberVariableItem> MemberVariables;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── ReadMemberDefaults ───

struct FBlueprintHelperReadMemberDefaultsResultData
{
	FString Schema = TEXT("ReadMemberDefaults.v1");
	TSharedPtr<FJsonObject> Values;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── Add/Set/Remove single result ───

struct FBlueprintHelperVariableSingleResult
{
	bool bSuccess = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("success"), bSuccess);
		return J;
	}
};

// ─── AddVariable ───

struct FBlueprintHelperAddMemberVariableResultData
{
	FString Schema = TEXT("AddMemberVariable.v1");
	FBlueprintHelperVariableSingleResult AddResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── Batch result (add/remove variables) ───

struct FBlueprintHelperVariableBatchResult
{
	int32 RequestedCount = 0, AddedCount = 0, RemovedCount = 0;
	int32 ChangedCount = 0, NoOpCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("requested_count"), RequestedCount);
		J->SetNumberField(TEXT("added_count"), AddedCount);
		J->SetNumberField(TEXT("removed_count"), RemovedCount);
		J->SetNumberField(TEXT("changed_count"), ChangedCount);
		J->SetNumberField(TEXT("no_op_count"), NoOpCount);
		return J;
	}
};

struct FBlueprintHelperAddMemberVariablesResultData
{
	FString Schema = TEXT("AddMemberVariables.v1");
	FBlueprintHelperVariableBatchResult AddResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── SetDefaults ───

struct FBlueprintHelperSetMemberDefaultsResultData
{
	FString Schema = TEXT("SetMemberDefault.v1");
	int32 AppliedCount = 0, ChangedCount = 0, NoOpCount = 0;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperSetMemberDefaultsBatchResultData
{
	FString Schema = TEXT("SetMemberDefaults.v1");
	FBlueprintHelperVariableBatchResult DefaultsResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── RemoveVariable ───

struct FBlueprintHelperRemoveMemberVariableResultData
{
	FString Schema = TEXT("RemoveMemberVariable.v1");
	FBlueprintHelperVariableSingleResult RemoveResult;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRemoveMemberVariablesResultData
{
	FString Schema = TEXT("RemoveMemberVariables.v1");
	FBlueprintHelperVariableBatchResult RemoveResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── LocalVariable ───

struct FBlueprintHelperLocalVariableItem
{
	FString VariableName;
	FBlueprintHelperVariableType VariableType;
	TOptional<FString> DefaultValue;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("variable_name"), VariableName);
		J->SetObjectField(TEXT("variable_type"), VariableType.ToJson());
		if (DefaultValue.IsSet()) J->SetStringField(TEXT("default_value"), *DefaultValue);
		return J;
	}
};

// ─── ReadLocalVariables ───

struct FBlueprintHelperReadLocalVariablesResultData
{
	FString Schema = TEXT("ReadLocalVariables.v1");
	FString FunctionName;
	TArray<FBlueprintHelperLocalVariableItem> LocalVariables;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── AddLocalVariable ───

struct FBlueprintHelperAddLocalVariableResultData
{
	FString Schema = TEXT("AddLocalVariable.v1");
	FBlueprintHelperVariableSingleResult AddResult;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAddLocalVariablesResultData
{
	FString Schema = TEXT("AddLocalVariables.v1");
	FBlueprintHelperVariableBatchResult AddResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── SetLocalVariableProperties ───

struct FBlueprintHelperSetLocalVariablePropertiesResultData
{
	FString Schema = TEXT("SetLocalVariableProperties.v1");
	FBlueprintHelperVariableBatchResult PropertiesResult;

	TSharedRef<FJsonObject> ToJson() const;
};

// ─── RemoveLocalVariable ───

struct FBlueprintHelperRemoveLocalVariableResultData
{
	FString Schema = TEXT("RemoveLocalVariable.v1");
	FBlueprintHelperVariableSingleResult RemoveResult;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRemoveLocalVariablesResultData
{
	FString Schema = TEXT("RemoveLocalVariables.v1");
	FBlueprintHelperVariableBatchResult RemoveResult;

	TSharedRef<FJsonObject> ToJson() const;
};
