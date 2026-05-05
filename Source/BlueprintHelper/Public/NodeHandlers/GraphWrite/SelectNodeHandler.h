#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * Select 节点处理器，处理 K2Node_Select 类型。
 * v2.9 。支持条件值选择节点。
 */
class FSelectNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
