#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * Switch 节点处理器，统一处理 SwitchInteger/SwitchString/SwitchName/SwitchEnum 类型。
 * v2.9 — 支持条件分支的 Switch 流程控制节点。
 */
class FSwitchNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
