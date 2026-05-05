#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * v2.3 。K2Node_GetEnumeratorName / K2Node_GetEnumeratorNameAsString 处理器。
 * 一个处理器同时处理枚举名和枚举名字符串两种节点。
 */
class FEnumNameNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
