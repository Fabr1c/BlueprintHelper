// BlueprintHelper Bridge 层 —— 通用命令路由与服务器辅助函数

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BlueprintHelperBridgeUtils.generated.h"

struct FBlueprintHelperBridgeRequest;
struct FBlueprintHelperBridgeResponse;
struct FBlueprintHelperToolResultBase;
enum class EBlueprintHelperTargetType : uint8;
struct FBlueprintHelperTargetRef;
struct FBlueprintHelperBridgeRuntimeConfig;
struct FBlueprintHelperReviewActionResult;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperBridgeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 使用指定端口构造 Bridge 运行时配置 */
	static FBlueprintHelperBridgeRuntimeConfig BridgeConfigWithPort(int32 InPort);

	/** 将工具执行结果包装为 Bridge 响应 */
	static FBlueprintHelperBridgeResponse MakeToolResultBridgeResponse(
		const FBlueprintHelperBridgeRequest& Req,
		const FBlueprintHelperToolResultBase& Result);

	/** 从字符串解析目标类型 */
	static EBlueprintHelperTargetType ParseBridgeTargetType(const FString& Type);

	/** 从逻辑作用域字符串解析目标类型 */
	static EBlueprintHelperTargetType ParseLogicScopeTargetType(const FString& Scope);

	/** 根据已填充的字段推断目标类型 */
	static EBlueprintHelperTargetType InferTargetTypeFromReadFields(const FBlueprintHelperTargetRef& Target);

	/** 将目标名称应用到类型化字段 */
	static void ApplyTargetNameToTypedField(FBlueprintHelperTargetRef& Target, const FString& TargetName);

	/** 从 JSON Payload 读取 TargetRef */
	static FBlueprintHelperTargetRef ReadTargetRefFromPayload(const TSharedPtr<FJsonObject>& Payload);

	/** 从 JSON Payload 读取字符串数组字段 */
	static TArray<FString> ReadStringArrayField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName);

	/** 将 ReviewActionResult 转换为 JSON */
	static TSharedRef<FJsonObject> ReviewActionResultToJson(const FBlueprintHelperReviewActionResult& Result);
};
