#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * FormatText 节点处理器，处理 K2Node_FormatText 类型。
 * 生成格式化文本节点，支持格式字符串和动态参数引脚。
 */
class FFormatTextNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
