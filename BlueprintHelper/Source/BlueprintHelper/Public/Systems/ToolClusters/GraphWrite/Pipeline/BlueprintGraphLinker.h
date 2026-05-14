#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class BLUEPRINTHELPER_API FBlueprintGraphLinker
{
public:
	static int32 ConnectComposerExecChain(UEdGraph* TargetGraph, const TArray<FParsedLink>& ParsedLinks, const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics);
	static int32 ConnectExplicitLinks(UEdGraph* TargetGraph, const TArray<FParsedLink>& ParsedLinks, const TMap<FString, UK2Node*>& IdToSpawnedNode, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics);
	static int32 ConnectFragmentDataEdges(UEdGraph* TargetGraph, const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, const TArray<FBlueprintHelperGraphFragmentDataEdge>& DataEdges, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics);
};
