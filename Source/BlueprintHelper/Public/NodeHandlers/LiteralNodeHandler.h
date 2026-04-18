#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * v2.3 — K2Node_Literal 处理器（对象引用常量节点）。
 */
class FLiteralNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
