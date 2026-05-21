#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphGenerationPipeline
{
public:
	static FBlueprintGenerateResult GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
	static FBlueprintGenerateResult GenerateSemanticGraphForGraph(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);
};
