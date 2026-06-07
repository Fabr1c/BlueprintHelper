#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UK2Node;

class BLUEPRINTHELPER_API FBlueprintGraphExistingNodeMapper
{
public:
	static void MapExistingNodeRefs(UEdGraph* TargetGraph, const TSharedPtr<class FJsonObject>& GraphJsonObject, TMap<FString, UK2Node*>& IdToSpawnedNode);
};
