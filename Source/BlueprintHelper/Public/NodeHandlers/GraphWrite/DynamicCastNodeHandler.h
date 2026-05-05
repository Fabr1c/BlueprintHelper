#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * DynamicCast 节点处理器，处理 K2Node_DynamicCast 类型。
 * 生成类型转换节点，支。cast_to 指定目标类路径。
 */
class FDynamicCastNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
