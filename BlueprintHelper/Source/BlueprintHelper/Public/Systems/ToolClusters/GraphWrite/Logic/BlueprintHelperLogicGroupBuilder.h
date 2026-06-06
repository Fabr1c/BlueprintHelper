// BlueprintHelper Service Layer — Logic Group Builder

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"

class FJsonObject;

/**
 * 逻辑分组构建器。
 * 从 RawJson 导出数据中扫描并构建 groups / entries / nodes / links。
 * 供 LogicMD / LogicJson 共用。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicGroupBuilder
{
public:
	FBlueprintHelperLogicGroupBuilder();

	/**
	 * 从 RawJson 对象构建分组。
	 * @param RawJson 来自 ExportService 导出的 Raw JSON。
	 * @param AssetPath 资产路径（用于填充局部上下文）。
	 * @param GraphName 图表名。
	 * @param Scope 当前 scope。
	 */
	FBlueprintHelperLogicJsonPayload BuildGroups(
		const TSharedPtr<FJsonObject>& RawJson,
		const FString& AssetPath,
		const FString& GraphName,
		EBlueprintHelperLogicScope Scope) const;

	/**
	 * 从 RawJson 中按入口名称构建单入口 payload。
	 * 支持完整蓝图 graphs[]：优先在 GraphName 内查找，GraphName 为空时扫描所有图表。
	 */
	FBlueprintHelperLogicJsonPayload BuildTargetEntry(
		const TSharedPtr<FJsonObject>& RawJson,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& TargetName,
		EBlueprintHelperLogicScope Scope) const;

	/** 判断当前 scope 是否为多入口 scope。 */
	static bool IsMultiEntryScope(EBlueprintHelperLogicScope Scope);

private:
	/** 尝试识别节点的语义类型。 */
	static EBlueprintHelperLogicNodeKind IdentifyNodeKind(const TSharedPtr<FJsonObject>& NodeObj);

	/** 将 Raw JSON 中的节点转换为 LogicNode。 */
	static FBlueprintHelperLogicNode ConvertNode(
		const TSharedPtr<FJsonObject>& NodeObj,
		int32 Index,
		const FString& AssetPath);

	/** 提取节点名。 */
	static FString ExtractNodeName(const TSharedPtr<FJsonObject>& NodeObj);

	/** 提取 owner（组件或对象）。 */
	static FString ExtractOwner(const TSharedPtr<FJsonObject>& NodeObj);

	/** 判断是否为入口节点。 */
	static bool IsEntryNode(const TSharedPtr<FJsonObject>& NodeObj);
};
