// BlueprintHelper shared bridge protocol types.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperBridgeError : uint8
{
	None,
	InvalidRequest,
	UnknownCommand,
	EditorNotReady,
	AssetNotFound,
	GraphNotFound,
	JsonParseFailed,
	Unauthorized,
	CommandDisabled,
	ExecutionFailed,
	InternalError
};

inline const TCHAR* BridgeErrorToString(EBlueprintHelperBridgeError Error)
{
	switch (Error)
	{
	case EBlueprintHelperBridgeError::None:             return TEXT("none");
	case EBlueprintHelperBridgeError::InvalidRequest:   return TEXT("invalid_request");
	case EBlueprintHelperBridgeError::UnknownCommand:   return TEXT("unknown_command");
	case EBlueprintHelperBridgeError::EditorNotReady:   return TEXT("editor_not_ready");
	case EBlueprintHelperBridgeError::AssetNotFound:    return TEXT("asset_not_found");
	case EBlueprintHelperBridgeError::GraphNotFound:    return TEXT("graph_not_found");
	case EBlueprintHelperBridgeError::JsonParseFailed:  return TEXT("json_parse_failed");
	case EBlueprintHelperBridgeError::Unauthorized:     return TEXT("unauthorized");
	case EBlueprintHelperBridgeError::CommandDisabled:  return TEXT("command_disabled");
	case EBlueprintHelperBridgeError::ExecutionFailed:  return TEXT("execution_failed");
	case EBlueprintHelperBridgeError::InternalError:    return TEXT("internal_error");
	default:                                            return TEXT("unknown");
	}
}

struct FBlueprintHelperBridgeRequest
{
	FString RequestId;
	FString Command;
	FString AuthSession;
	TSharedPtr<FJsonObject> Payload;
};

struct FBlueprintHelperBridgeResponse
{
	FString RequestId;
	bool bSuccess = false;
	EBlueprintHelperBridgeError ErrorCode = EBlueprintHelperBridgeError::None;
	FString Message;
	TSharedPtr<FJsonObject> Result;

	static FBlueprintHelperBridgeResponse Success(const FString& InRequestId, const FString& InMessage = TEXT(""))
	{
		FBlueprintHelperBridgeResponse Resp;
		Resp.RequestId = InRequestId;
		Resp.bSuccess = true;
		Resp.Message = InMessage;
		return Resp;
	}

	static FBlueprintHelperBridgeResponse Error(
		const FString& InRequestId,
		EBlueprintHelperBridgeError InError,
		const FString& InMessage)
	{
		FBlueprintHelperBridgeResponse Resp;
		Resp.RequestId = InRequestId;
		Resp.bSuccess = false;
		Resp.ErrorCode = InError;
		Resp.Message = InMessage;
		return Resp;
	}
};

struct FBlueprintHelperEditorContext
{
	FString ActiveBlueprintPath;
	FString ActiveGraphName;
	FString BlueprintDisplayName;
	int32 NodeCount = 0;
	bool bIsCompiled = false;
	int32 BlueprintStatus = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeValidationError
{
	FString Code;
	FString Field;
	FString ExpectedType;
	FString ActualType;
	FString Message;
};
