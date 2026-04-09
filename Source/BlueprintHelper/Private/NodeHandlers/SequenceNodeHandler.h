#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * Sequence 节点处理器，处理 K2Node_ExecutionSequence 类型。
 */
class FSequenceNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
