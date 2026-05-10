// BlueprintHelper Service Layer — LogicJson Read Service

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"

struct FBlueprintHelperLogicJsonData;
struct FBlueprintHelperTargetRef;

/**
 * LogicJson 只读服务。
 * 根据 target 读取蓝图逻辑并以结构化 JSON 返回。
 * 多入口 scope 下返回 groups[]，单入口 scope 返回 entry+nodes。
 * 不负责：导入、导出、写入、Transaction、Review。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicJsonReadService
{
public:
	FBlueprintHelperLogicJsonReadService();

	/** 根据 TargetRef 读取 LogicJson。 */
	FBlueprintHelperLogicJsonData ReadLogicJson(const FBlueprintHelperTargetRef& Target) const;

private:
	/** 从 TargetType 推断 LogicScope。 */
	static EBlueprintHelperLogicScope TargetTypeToScope(EBlueprintHelperTargetType Type);

	/** 解析 scope → 对应 ExportScope。 */
	static EBlueprintHelperExportScope ScopeToExportScope(EBlueprintHelperLogicScope Scope);

	/** LogicGroupBuilder 实例。 */
	FBlueprintHelperLogicGroupBuilder GroupBuilder;
};
