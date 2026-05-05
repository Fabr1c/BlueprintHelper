#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * 可提升运算符节点处理器，处理 K2Node_PromotableOperator 类型。
 * v2.9 。支持 UE5 的加减乘除、比较等数学运算符节点。
 */
class FPromotableOperatorNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
