#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * 宏节点处理器，处理 K2Node_MacroInstance 类型。
 */
class FMacroInstanceNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
