// BlueprintHelper Bridge Layer — 协议类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 错误码 ───

/** Bridge 层错误码。 */
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

/** 错误码 → 字符串。 */
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

// ─── Bridge 请求 ───

/** 外部客户端发送的请求。 */
struct FBlueprintHelperBridgeRequest
{
	/** 请求 ID，用于幂等与追踪。 */
	FString RequestId;

	/** 命令名称，例如 "import_json"、"compile_blueprint"。 */
	FString Command;
	/** Approved short-lived write session. Write and high-risk commands require this. */
	FString AuthSession;

	/** 业务参数。 */
	TSharedPtr<FJsonObject> Payload;
};

// ─── Bridge 响应 ───

/** 返回给客户端的响应。 */
struct FBlueprintHelperBridgeResponse
{
	FString RequestId;
	bool bSuccess = false;
	EBlueprintHelperBridgeError ErrorCode = EBlueprintHelperBridgeError::None;
	FString Message;

	/** 业务结果（各命令不同）。 */
	TSharedPtr<FJsonObject> Result;

	/** 快速构造成功响应。 */
	static FBlueprintHelperBridgeResponse Success(const FString& InRequestId, const FString& InMessage = TEXT(""))
	{
		FBlueprintHelperBridgeResponse Resp;
		Resp.RequestId = InRequestId;
		Resp.bSuccess = true;
		Resp.Message = InMessage;
		return Resp;
	}

	/** 快速构造错误响应。 */
	static FBlueprintHelperBridgeResponse Error(const FString& InRequestId,
		EBlueprintHelperBridgeError InError, const FString& InMessage)
	{
		FBlueprintHelperBridgeResponse Resp;
		Resp.RequestId = InRequestId;
		Resp.bSuccess = false;
		Resp.ErrorCode = InError;
		Resp.Message = InMessage;
		return Resp;
	}
};

// ─── 编辑器上下文 ───

/** 当前编辑器上下文快照。 */
struct FBlueprintHelperEditorContext
{
	FString ActiveBlueprintPath;
	FString ActiveGraphName;
	FString BlueprintDisplayName;
	int32 NodeCount = 0;
	bool bIsCompiled = false;
	int32 BlueprintStatus = 0;
};
