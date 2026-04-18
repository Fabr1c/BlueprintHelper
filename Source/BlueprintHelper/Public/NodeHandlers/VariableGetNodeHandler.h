#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * 变量读取节点处理器，处理 K2Node_VariableGet 类型。
 */
class FVariableGetNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
