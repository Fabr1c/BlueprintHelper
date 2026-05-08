#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * 变量写入节点处理器，处理 K2Node_VariableSet 类型。
 */
class FVariableSetNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
