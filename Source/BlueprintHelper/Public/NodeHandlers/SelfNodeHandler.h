#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * Self 节点处理器，处理 K2Node_Self 类型。
 * 生成一个返回当前蓝图实例引用的纯节点。
 */
class FSelfNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
