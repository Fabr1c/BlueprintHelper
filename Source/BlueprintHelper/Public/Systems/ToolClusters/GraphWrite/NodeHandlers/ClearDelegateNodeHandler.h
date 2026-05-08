#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * ClearDelegate 节点处理器，处理 K2Node_ClearDelegate 类型。
 * 解绑事件分发器的所有绑定。
 */
class FClearDelegateNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
