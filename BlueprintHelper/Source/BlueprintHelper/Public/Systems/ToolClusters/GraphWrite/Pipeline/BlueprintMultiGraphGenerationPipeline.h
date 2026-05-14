#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintMultiGraphGenerationPipeline
{
public:
	static FBlueprintGenerateResult GenerateMultiGraphFromJson( UBlueprint* Blueprint, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
};
