// BlueprintHelper Service Layer — SaveAsset 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperSaveErrorCode : uint8
{
	InvalidRequest, AssetNotFound, AssetLoadFailed, PackageNotFound,
	PackageNotWritable, SaveFailed, FileLocked, SourceControlCheckoutFailed,
	BridgeDisconnected, InternalError
};

inline const TCHAR* SaveErrorCodeToString(EBlueprintHelperSaveErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperSaveErrorCode::InvalidRequest:               return TEXT("invalid_request");
	case EBlueprintHelperSaveErrorCode::AssetNotFound:                return TEXT("asset_not_found");
	case EBlueprintHelperSaveErrorCode::AssetLoadFailed:              return TEXT("asset_load_failed");
	case EBlueprintHelperSaveErrorCode::PackageNotFound:              return TEXT("package_not_found");
	case EBlueprintHelperSaveErrorCode::PackageNotWritable:           return TEXT("package_not_writable");
	case EBlueprintHelperSaveErrorCode::SaveFailed:                   return TEXT("save_failed");
	case EBlueprintHelperSaveErrorCode::FileLocked:                   return TEXT("file_locked");
	case EBlueprintHelperSaveErrorCode::SourceControlCheckoutFailed:  return TEXT("source_control_checkout_failed");
	case EBlueprintHelperSaveErrorCode::BridgeDisconnected:           return TEXT("bridge_disconnected");
	case EBlueprintHelperSaveErrorCode::InternalError:                return TEXT("internal_error");
	default:                                                            return TEXT("unknown");
	}
}

// ─── 结果结构（无 write_ref/validation） ───

struct FBlueprintHelperSaveAssetResult
{
	bool bSaved = false;
	bool bWasDirty = false;
	TOptional<FString> Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("saved"), bSaved);
		J->SetBoolField(TEXT("was_dirty"), bWasDirty);
		if (Reason.IsSet()) J->SetStringField(TEXT("reason"), *Reason);
		return J;
	}
};

struct FBlueprintHelperSaveAssetResultData
{
	FString Schema = TEXT("SaveAsset.v1");
	FBlueprintHelperSaveAssetResult SaveResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("save_result"), SaveResult.ToJson());
		return J;
	}
};
