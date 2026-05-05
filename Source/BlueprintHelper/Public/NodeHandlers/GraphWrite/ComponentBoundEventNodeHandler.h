#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * v2.3 。K2Node_ComponentBoundEvent 处理器（组件绑定事件节点）。
 */
class FComponentBoundEventNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
