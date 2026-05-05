#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * Event 节点处理器，处理 K2Node_Event 类型。
 * 生成引擎预定义事件节点（BeginPlay、Tick 等）。
 */
class FEventNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
