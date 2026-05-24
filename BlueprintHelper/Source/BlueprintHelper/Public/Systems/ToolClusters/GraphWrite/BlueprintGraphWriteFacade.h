#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

class UBlueprint;
class UEdGraph;
class UK2Node;

class BLUEPRINTHELPER_API FBlueprintGraphWriteFacade
{
public:
	static FBlueprintHelperCallFunctionResolveResult ResolveFunctionForGraph(UEdGraph* TargetGraph, const FString& FunctionQuery, const TMap<FString, FString>& DefaultValues);
	static FBlueprintHelperActionResolutionResult ResolveActionForGraph(const FBlueprintHelperActionResolutionRequest& Request);
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);
	static TArray<TSharedPtr<FEngineFunctionItem>> GetAllBlueprintFunctions();
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues(UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues, const FString& NodeId = TEXT(""));
	static class UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);
};
