// BlueprintHelper Service Layer — Project Context / Marker / Setup State 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperProjectReadScope : uint8 { ProjectContext, ProjectMarker, SetupState };
inline const TCHAR* ProjectReadScopeToString(EBlueprintHelperProjectReadScope S)
{
	switch (S) { case EBlueprintHelperProjectReadScope::ProjectContext: return TEXT("project_context"); case EBlueprintHelperProjectReadScope::ProjectMarker: return TEXT("project_marker"); case EBlueprintHelperProjectReadScope::SetupState: return TEXT("setup_state"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperProjectContextErrorCode : uint8
{
	InvalidRequest, ProjectContextCheckFailed, ProjectMarkerCheckFailed,
	SetupStateCheckFailed, MarkerFileUnreadable, SettingsStateUnavailable, InternalError
};
inline const TCHAR* ProjectContextErrorCodeToString(EBlueprintHelperProjectContextErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperProjectContextErrorCode::InvalidRequest:             return TEXT("invalid_request");
	case EBlueprintHelperProjectContextErrorCode::ProjectContextCheckFailed:   return TEXT("project_context_check_failed");
	case EBlueprintHelperProjectContextErrorCode::ProjectMarkerCheckFailed:    return TEXT("project_marker_check_failed");
	case EBlueprintHelperProjectContextErrorCode::SetupStateCheckFailed:       return TEXT("setup_state_check_failed");
	case EBlueprintHelperProjectContextErrorCode::MarkerFileUnreadable:        return TEXT("marker_file_unreadable");
	case EBlueprintHelperProjectContextErrorCode::SettingsStateUnavailable:    return TEXT("settings_state_unavailable");
	case EBlueprintHelperProjectContextErrorCode::InternalError:               return TEXT("internal_error");
	default:                                                                     return TEXT("unknown");
	}
}

// ─── ReadProjectContext ───

struct FBlueprintHelperProjectContextStatusData
{
	FString Status; // ok | degraded | blocked
	bool bProjectDetected = false;
	TOptional<FString> ProjectMarker; // present | missing | invalid
	bool bWorkflowEnabled = false;
	TOptional<FString> Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("status"), Status);
		J->SetBoolField(TEXT("project_detected"), bProjectDetected);
		if (ProjectMarker.IsSet()) J->SetStringField(TEXT("project_marker"), *ProjectMarker);
		J->SetBoolField(TEXT("workflow_enabled"), bWorkflowEnabled);
		if (Reason.IsSet()) J->SetStringField(TEXT("reason"), *Reason);
		return J;
	}
};

struct FBlueprintHelperReadProjectContextResultData
{
	FString Schema = TEXT("ReadProjectContext.v1");
	FBlueprintHelperProjectContextStatusData ProjectContext;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("project_context"), ProjectContext.ToJson());
		return J;
	}
};

// ─── CheckProjectMarker ───

struct FBlueprintHelperProjectMarkerCheck
{
	FString Status; // present | missing | invalid
	bool bWorkflowEnabled = false;
	TOptional<FString> Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("status"), Status);
		J->SetBoolField(TEXT("workflow_enabled"), bWorkflowEnabled);
		if (Reason.IsSet()) J->SetStringField(TEXT("reason"), *Reason);
		return J;
	}
};

struct FBlueprintHelperCheckProjectMarkerResultData
{
	FString Schema = TEXT("CheckProjectMarker.v1");
	FBlueprintHelperProjectMarkerCheck ProjectMarker;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("project_marker"), ProjectMarker.ToJson());
		return J;
	}
};

// ─── CheckSetupState ───

struct FBlueprintHelperSetupState
{
	FString Status; // ok | blocked
	TOptional<FString> Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("status"), Status);
		if (Reason.IsSet()) J->SetStringField(TEXT("reason"), *Reason);
		return J;
	}
};

struct FBlueprintHelperCheckSetupStateResultData
{
	FString Schema = TEXT("CheckSetupState.v1");
	FBlueprintHelperSetupState SetupState;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("setup_state"), SetupState.ToJson());
		return J;
	}
};
