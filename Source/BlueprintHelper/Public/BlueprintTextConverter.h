#pragma once

#include "CoreMinimal.h"

// 引脚信息
struct FMinimalPin
{
	FString PinId;
	FString PinName;
	FString PinCategory; // e.g., exec, string, bool
	FString PinSubCategory;
	FString PinSubCategoryObjectPath;
	FString ContainerType;
	FString DefaultValue;
	bool bIsOutput = false; // 是否是输出引脚
	bool bIsReference = false;
	bool bIsConst = false;
	TArray<FString> RawLinkedTo; // 原始连线数据，例如 "K2Node_CallFunction_0 3E4A..."
};

// 节点信息
struct FMinimalNode
{
	FString InternalName;  // e.g., K2Node_CallFunction_0
	FString NodeType;      // e.g., CallFunction, Event
	FString DisplayName;   // e.g., PrintString, EventTick
	FString VariableScopeType;
	FString VariableScopeGraphName;
	FString VariableOwnerClassPath;
	FString MacroName;
	FString MacroAssetPath;
	bool bSelfContext = true;
	int32 NodePosX = 0;
	int32 NodePosY = 0;
	TArray<FMinimalPin> Inputs;
	TArray<FMinimalPin> Outputs;
};

/**
 * Blueprint 文本转换器，负责在 Blueprint T3D 与插件约束 JSON 之间转换。
 */
class FBlueprintToTextConverter
{
public:
	/** 兼容旧接口：读取剪贴板蓝图并返回插件可回放 JSON。 */
	static FString ConvertClipboardToMinimalText();

	/** 读取剪贴板���图并转换为插件可回放 JSON。 */
	static FString ConvertClipboardToJson();

	/** 将传入的 Blueprint T3D 文本转换为插件可回放 JSON。 */
	static FString ConvertTextToJson(const FString& T3DText);

	/** 判断文本是否为 Blueprint T3D 导出内容。 */
	static bool IsBlueprintT3DText(const FString& SourceText);
	/** 从图表对象直接导出节点和连线为 JSON。v2.1 */
	static FString ConvertGraphToJson(class UEdGraph* TargetGraph);

	/** 导出完整蓝图结构为 JSON（所有变量 + 函数签名 + 事件分发器 + 所有图表节点/连线）。v2.1 */
	static FString ExportBlueprintToJson(class UBlueprint* Blueprint);
private:
	/** 解析 T3D 文本到轻量节点结构。 */
	static void ParseT3DToNodes(const FString& T3DText, TMap<FString, FMinimalNode>& OutNodes);

	/** 将轻量节点结构格式化为插件约束 JSON。 */
	static FString FormatNodesToJson(const TMap<FString, FMinimalNode>& Nodes);

	/** 解析变量节点引用描述。 */
	static void ParseVariableReferenceLine(const FString& Line, FMinimalNode& Node);

	/** 解析宏节点引用描述。 */
	static void ParseMacroReferenceLine(const FString& Line, FMinimalNode& Node);

	/** 清洗 T3D 导出的默认值文本，便于写回 JSON。 */
	static FString NormalizeExportValue(const FString& InValue);

	/** 将 EdGraphPinType 转换为 JSON pin_type 对象。v2.1 */
	static TSharedPtr<class FJsonObject> PinTypeToJson(const struct FEdGraphPinType& PinType);

	/** 导出单个图表的节点和连线为 JSON 数组。v2.1 */
	static void ExportGraphNodesAndLinks(class UEdGraph* Graph, TArray<TSharedPtr<class FJsonValue>>& OutNodes, TArray<TSharedPtr<class FJsonValue>>& OutLinks);

	/** 识别 K2Node 的类型字符串。v2.1 */
	static FString IdentifyNodeType(class UEdGraphNode* Node);
};
