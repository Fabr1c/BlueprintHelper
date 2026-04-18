#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * CallDelegate 节点处理器，处理 K2Node_CallDelegate 类型。
 * 调用事件分发器。
 */
class FCallDelegateNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
