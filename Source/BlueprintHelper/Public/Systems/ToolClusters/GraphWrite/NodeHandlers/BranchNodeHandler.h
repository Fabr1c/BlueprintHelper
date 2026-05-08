#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * Branch 节点处理器，处理 K2Node_IfThenElse 类型。
 */
class FBranchNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
