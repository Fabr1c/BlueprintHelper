// BlueprintHelper Service Layer — Editor Lifecycle / PIE / Risk Command 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperLifecycleScope : uint8 { Editor, Pie };
inline const TCHAR* LifecycleScopeToString(EBlueprintHelperLifecycleScope S)
{
	switch (S) { case EBlueprintHelperLifecycleScope::Editor: return TEXT("editor"); case EBlueprintHelperLifecycleScope::Pie: return TEXT("pie"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperLifecycleErrorCode : uint8
{
	InvalidRequest, EditorUnavailable, PieStartFailed, PieStopFailed,
	BlueprintCompileErrorsExist, RiskCommandMissing, RiskCommandInvalid,
	CommandNotAuthorized, UnsavedAssetsExist, CloseEditorDryRunRequired,
	CloseEditorFailed, InternalError
};
inline const TCHAR* LifecycleErrorCodeToString(EBlueprintHelperLifecycleErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperLifecycleErrorCode::InvalidRequest:              return TEXT("invalid_request");
	case EBlueprintHelperLifecycleErrorCode::EditorUnavailable:           return TEXT("editor_unavailable");
	case EBlueprintHelperLifecycleErrorCode::PieStartFailed:             return TEXT("pie_start_failed");
	case EBlueprintHelperLifecycleErrorCode::PieStopFailed:              return TEXT("pie_stop_failed");
	case EBlueprintHelperLifecycleErrorCode::BlueprintCompileErrorsExist: return TEXT("blueprint_compile_errors_exist");
	case EBlueprintHelperLifecycleErrorCode::RiskCommandMissing:          return TEXT("risk_command_missing");
	case EBlueprintHelperLifecycleErrorCode::RiskCommandInvalid:          return TEXT("risk_command_invalid");
	case EBlueprintHelperLifecycleErrorCode::CommandNotAuthorized:        return TEXT("command_not_authorized");
	case EBlueprintHelperLifecycleErrorCode::UnsavedAssetsExist:          return TEXT("unsaved_assets_exist");
	case EBlueprintHelperLifecycleErrorCode::CloseEditorDryRunRequired:   return TEXT("close_editor_dry_run_required");
	case EBlueprintHelperLifecycleErrorCode::CloseEditorFailed:           return TEXT("close_editor_failed");
	case EBlueprintHelperLifecycleErrorCode::InternalError:               return TEXT("internal_error");
	default:                                                                 return TEXT("unknown");
	}
}

// ─── EditorLifecycleStatus ───

struct FBlueprintHelperEditorLifecycleStatus
{
	bool bEditorRunning = false, bPieRunning = false;
	int32 UnsavedAssetCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("editor_running"), bEditorRunning);
		J->SetBoolField(TEXT("pie_running"), bPieRunning);
		J->SetNumberField(TEXT("unsaved_asset_count"), UnsavedAssetCount);
		return J;
	}
};

struct FBlueprintHelperEditorLifecycleStatusData
{
	FString Schema = TEXT("EditorLifecycleStatus.v1");
	FBlueprintHelperEditorLifecycleStatus EditorLifecycle;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("editor_lifecycle"), EditorLifecycle.ToJson());
		return J;
	}
};

// ─── PIE Result ───

struct FBlueprintHelperPieResult
{
	TOptional<bool> bStarted, bAlreadyRunning;
	TOptional<bool> bStopped, bWasRunning;
	TOptional<FString> Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		if (bStarted.IsSet()) { J->SetBoolField(TEXT("started"), *bStarted); J->SetBoolField(TEXT("already_running"), bAlreadyRunning.Get(false)); }
		if (bStopped.IsSet()) { J->SetBoolField(TEXT("stopped"), *bStopped); J->SetBoolField(TEXT("was_running"), bWasRunning.Get(false)); }
		if (Reason.IsSet()) J->SetStringField(TEXT("reason"), *Reason);
		return J;
	}
};

// ─── CloseEditor ───

struct FBlueprintHelperCloseEditorResult
{
	bool bCloseRequested = false;
	TOptional<bool> bAlreadyClosing;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("close_requested"), bCloseRequested);
		if (bAlreadyClosing.IsSet()) J->SetBoolField(TEXT("already_closing"), *bAlreadyClosing);
		return J;
	}
};

struct FBlueprintHelperCloseEditorResultData
{
	FString Schema = TEXT("CloseEditor.v1");
	FBlueprintHelperCloseEditorResult CloseResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("close_result"), CloseResult.ToJson());
		return J;
	}
};
