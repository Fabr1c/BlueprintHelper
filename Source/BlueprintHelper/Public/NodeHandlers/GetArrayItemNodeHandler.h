#pragma once

#include "CoreMinimal.h"
#include "BlueprintNodeHandler.h"

/** K2Node_GetArrayItem 节点处理器 —— 纯节点，通过索引获取数组元素 */
class FGetArrayItemNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool  CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
