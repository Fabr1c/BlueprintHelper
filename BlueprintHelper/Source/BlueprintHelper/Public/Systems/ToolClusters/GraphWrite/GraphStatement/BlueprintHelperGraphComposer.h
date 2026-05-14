#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphComposeResult
{
	bool bSucceeded = false;
	int32 CreatedExecConnectionCount = 0;
	int32 CreatedDataConnectionCount = 0;
	TArray<FString> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphComposer
{
public:
	static FBlueprintHelperGraphComposeResult ConnectLinearExecChain(
		UEdGraph* TargetGraph,
		const TArray<FBlueprintHelperNodeFragment>& Fragments);

	static FBlueprintHelperGraphComposeResult ConnectDataEdges(
		UEdGraph* TargetGraph,
		const TArray<FBlueprintHelperNodeFragment>& Fragments,
		const TArray<FBlueprintHelperGraphFragmentDataEdge>& DataEdges);
};
