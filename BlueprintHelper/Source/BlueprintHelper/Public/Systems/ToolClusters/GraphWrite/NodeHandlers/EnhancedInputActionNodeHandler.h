#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * Enhanced Input Action 节点处理器，处理 K2Node_EnhancedInputAction 类型。
 * v2.9 。支持 EnhancedInput 系统的输入动作事件节点。
 */
class FEnhancedInputActionNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
