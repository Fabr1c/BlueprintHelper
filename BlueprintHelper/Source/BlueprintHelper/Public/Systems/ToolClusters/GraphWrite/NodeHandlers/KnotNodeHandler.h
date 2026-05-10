#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * v2.3 。K2Node_Knot 处理器（布线转接/重新路由节点）。
 */
class FKnotNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
