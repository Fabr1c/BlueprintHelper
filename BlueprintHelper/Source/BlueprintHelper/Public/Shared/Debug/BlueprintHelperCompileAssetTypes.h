// BlueprintHelper Service Layer — CompileBlueprintAsset 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperServiceTypes.h"

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
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	TArray<FBlueprintHelperDiagnosticItem> CompilerResults;
	TOptional<FString> Format;
	TOptional<FString> Markdown;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("success"), bSuccess);
		J->SetStringField(TEXT("status"), Status);
		J->SetNumberField(TEXT("error_count"), ErrorCount);
		J->SetNumberField(TEXT("warning_count"), WarningCount);
		if (CompilerResults.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ResultsArray;
			for (const FBlueprintHelperDiagnosticItem& Item : CompilerResults)
			{
				TSharedPtr<FJsonObject> ItemJson = MakeShared<FJsonObject>();
				switch (Item.Severity)
				{
				case EBlueprintHelperDiagnosticSeverity::Error:
					ItemJson->SetStringField(TEXT("severity"), TEXT("error"));
					break;
				case EBlueprintHelperDiagnosticSeverity::Warning:
					ItemJson->SetStringField(TEXT("severity"), TEXT("warning"));
					break;
				default:
					ItemJson->SetStringField(TEXT("severity"), TEXT("info"));
					break;
				}
				if (!Item.Code.IsEmpty()) ItemJson->SetStringField(TEXT("code"), Item.Code);
				if (!Item.Message.IsEmpty()) ItemJson->SetStringField(TEXT("message"), Item.Message);
				if (!Item.NodeId.IsEmpty()) ItemJson->SetStringField(TEXT("node_id"), Item.NodeId);
				if (!Item.NodeName.IsEmpty()) ItemJson->SetStringField(TEXT("node_name"), Item.NodeName);
				if (!Item.PinName.IsEmpty()) ItemJson->SetStringField(TEXT("pin_name"), Item.PinName);
				if (!Item.Field.IsEmpty()) ItemJson->SetStringField(TEXT("field"), Item.Field);
				ResultsArray.Add(MakeShared<FJsonValueObject>(ItemJson));
			}
			J->SetArrayField(TEXT("compiler_results"), ResultsArray);
		}
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
