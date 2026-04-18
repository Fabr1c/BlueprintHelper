#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * AssignDelegate 节点处理器，处理 K2Node_AssignDelegate 类型。
 * 快捷绑定：同时创建事件绑定和对应的自定义事件。
 */
class FAssignDelegateNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
