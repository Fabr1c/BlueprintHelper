#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * CustomEvent 节点处理器，处理 K2Node_CustomEvent 类型。
 * 生成自定义事件节点，支持自定义参数输出引脚。
 */
class FCustomEventNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
