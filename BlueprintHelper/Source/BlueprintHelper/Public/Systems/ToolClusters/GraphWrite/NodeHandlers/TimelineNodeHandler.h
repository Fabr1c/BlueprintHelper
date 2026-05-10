#pragma once

#include "CoreMinimal.h"
#include "BlueprintNodeHandler.h"

/** K2Node_Timeline 节点处理器 —— 在蓝图中创建时间轴节点及其模板 */
class FTimelineNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool  CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
