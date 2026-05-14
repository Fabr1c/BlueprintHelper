#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphExistingNodeMapper
{
public:
	static void MapFunctionEntryResultNodes(UEdGraph* TargetGraph, TMap<FString, UK2Node*>& IdToSpawnedNode);
	static void MapExistingNodeRefs(UEdGraph* TargetGraph, const TSharedPtr<class FJsonObject>& GraphJsonObject, TMap<FString, UK2Node*>& IdToSpawnedNode);
};
