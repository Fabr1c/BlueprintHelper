#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"

class FBlueprintGraphNodeSpawner
{
public:
	static UK2Node* SpawnMacroNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage);
};
