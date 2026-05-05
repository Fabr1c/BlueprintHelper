#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * CreateDelegate 节点处理器，处理 K2Node_CreateDelegate 类型。
 * 创建委托对象节点，可指定绑定的函数名。
 */
class FCreateDelegateNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
