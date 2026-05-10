#pragma once

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"

/**
 * SpawnActor 节点处理器，处理 K2Node_SpawnActorFromClass 类型。
 * 生成 SpawnActor 节点，通过 spawn.class_path 指定要生成的 Actor 类。
 */
class FSpawnActorNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;
};
