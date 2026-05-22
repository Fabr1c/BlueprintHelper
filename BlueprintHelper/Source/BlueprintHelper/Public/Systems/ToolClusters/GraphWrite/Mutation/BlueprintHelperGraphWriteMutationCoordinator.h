#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"

class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteMutationCoordinator
{
public:
	static FBlueprintGenerateResult ExecuteIntents(
		UEdGraph* TargetGraph,
		const TArray<FBlueprintHelperGraphWriteMutationIntent>& Intents,
		TArray<FString>& OutUnresolvedNodes);
};

