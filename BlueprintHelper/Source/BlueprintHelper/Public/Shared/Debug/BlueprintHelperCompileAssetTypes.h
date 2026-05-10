// BlueprintHelper Service Layer — CompileBlueprintAsset 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperCompileStatus : uint8 { Succeeded, Failed };

inline const TCHAR* CompileStatusToString(EBlueprintHelperCompileStatus S)
{
	switch (S) { case EBlueprintHelperCompileStatus::Succeeded: return TEXT("succeeded"); case EBlueprintHelperCompileStatus::Failed: return TEXT("failed"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperCompileToolErrorCode : uint8
{
	InvalidRequest, AssetNotFound, TargetNotBlueprint, BlueprintLoadFailed,
	CompileApiFailed, BridgeDisconnected, InternalError
};

inline const TCHAR* CompileToolErrorCodeToString(EBlueprintHelperCompileToolErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperCompileToolErrorCode::InvalidRequest:      return TEXT("invalid_request");
	case EBlueprintHelperCompileToolErrorCode::AssetNotFound:       return TEXT("asset_not_found");
	case EBlueprintHelperCompileToolErrorCode::TargetNotBlueprint:  return TEXT("target_not_blueprint");
	case EBlueprintHelperCompileToolErrorCode::BlueprintLoadFailed: return TEXT("blueprint_load_failed");
	case EBlueprintHelperCompileToolErrorCode::CompileApiFailed:    return TEXT("compile_api_failed");
	case EBlueprintHelperCompileToolErrorCode::BridgeDisconnected:  return TEXT("bridge_disconnected");
	case EBlueprintHelperCompileToolErrorCode::InternalError:       return TEXT("internal_error");
	default:                                                          return TEXT("unknown");
	}
}

// ─── 结果结构 ───

struct FBlueprintHelperCompileAssetResult
{
	bool bSuccess = false;
	FString Status = TEXT("succeeded");
	int32 WarningCount = 0;
	TOptional<FString> Format;
	TOptional<FString> Markdown;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("success"), bSuccess);
		J->SetStringField(TEXT("status"), Status);
		J->SetNumberField(TEXT("warning_count"), WarningCount);
		if (Format.IsSet()) J->SetStringField(TEXT("format"), *Format);
		if (Markdown.IsSet()) J->SetStringField(TEXT("markdown"), *Markdown);
		return J;
	}
};

struct FBlueprintHelperCompileAssetResultData
{
	FString Schema = TEXT("CompileBlueprintAsset.v1");
	FBlueprintHelperCompileAssetResult CompileResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("compile_result"), CompileResult.ToJson());
		return J;
	}
};
