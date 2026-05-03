// BlueprintHelper Service Layer — LogicMD Read Service

#pragma once

#include "CoreMinimal.h"
#include "Services/BlueprintHelperLogicMdTypes.h"
#include "Services/BlueprintHelperLogicGroupBuilder.h"
#include "Services/BlueprintHelperToolResultTypes.h"
#include "Services/BlueprintHelperServiceTypes.h"

struct FBlueprintHelperLogicMdData;
struct FBlueprintHelperTargetRef;

/**
 * LogicMD 只读服务。
 * 根据 target 读取蓝图逻辑并以 Agent 友好的 Markdown 格式返回。
 * 多入口 scope 下按 group 分段，并返回 grouped=true。
 * 不负责：导入、导出、写入、Transaction、Review。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicMdReadService
{
public:
	FBlueprintHelperLogicMdReadService();

	/**
	 * 根据 TargetRef 读取 LogicMD。
	 */
	FBlueprintHelperLogicMdData ReadLogicMd(const FBlueprintHelperTargetRef& Target) const;

private:
	/** 从 TargetType 推断 LogicScope。 */
	static EBlueprintHelperLogicScope TargetTypeToScope(EBlueprintHelperTargetType Type);

	/** 解析 scope → 对应 ExportScope。 */
	static EBlueprintHelperExportScope ScopeToExportScope(EBlueprintHelperLogicScope Scope);

	/** LogicGroupBuilder 实例。 */
	FBlueprintHelperLogicGroupBuilder GroupBuilder;
};
