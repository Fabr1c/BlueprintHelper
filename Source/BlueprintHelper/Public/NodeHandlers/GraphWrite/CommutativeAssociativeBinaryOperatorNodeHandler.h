#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * 交换结合律二元运算符节点处理器，处理 K2Node_CommutativeAssociativeBinaryOperator 类型。
 * v2.9 。支持可动态添加输入引脚的数学运算节点（如多参数加法、乘法等）。
 */
class FCommutativeAssociativeBinaryOperatorNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
