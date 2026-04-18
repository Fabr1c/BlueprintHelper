// BlueprintHelper Bridge Layer — 协议序列化/反序列化

#pragma once

#include "CoreMinimal.h"
#include "Bridge/BlueprintHelperBridgeTypes.h"

/**
 * Bridge 协议解析与序列化工具。
 * 负责 JSON 文本 ↔ Bridge DTO 的转换。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBridgeProtocol
{
public:
	/** 将 JSON 文本解析为 Bridge 请求。失败时返回空 TOptional。 */
	static TOptional<FBlueprintHelperBridgeRequest> ParseRequest(const FString& JsonText);

	/** 将 Bridge 响应序列化为 JSON 文本。 */
	static FString SerializeResponse(const FBlueprintHelperBridgeResponse& Response);

	/** 将 EditorContext 转为 JsonObject。 */
	static TSharedPtr<FJsonObject> ContextToJson(const FBlueprintHelperEditorContext& Ctx);
};
