// BlueprintHelper v1.5 — MakeArray / MakeSet / MakeMap Handler

#pragma once

#include "NodeHandlers/BlueprintNodeHandler.h"

/**
 * 处理容器构造节点：K2Node_MakeArray、K2Node_MakeSet、K2Node_MakeMap。
 */
class FMakeContainerNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;

private:
	UK2Node* SpawnMakeArray(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const;
	UK2Node* SpawnMakeSet(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const;
	UK2Node* SpawnMakeMap(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const;
};
