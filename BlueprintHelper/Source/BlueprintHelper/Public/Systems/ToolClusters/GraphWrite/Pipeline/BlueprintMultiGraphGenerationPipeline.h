#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

class UBlueprint;

class BLUEPRINTHELPER_API FBlueprintMultiGraphGenerationPipeline
{
public:
	static FBlueprintGenerateResult GenerateMultiGraphFromJson( UBlueprint* Blueprint, const FString& JsonString, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
};
