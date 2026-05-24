#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"

FBlueprintHelperCallFunctionResolveResult FBlueprintGraphWriteFacade::ResolveFunctionForGraph(
	UEdGraph* TargetGraph,
	const FString& FunctionQuery,
	const TMap<FString, FString>& DefaultValues)
{
	return FBlueprintGraphNodeUtility::ResolveFunctionForGraph(TargetGraph, FunctionQuery, DefaultValues);
}

FBlueprintHelperActionResolutionResult FBlueprintGraphWriteFacade::ResolveActionForGraph(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	return FBlueprintHelperActionResolutionCore::Resolve(Request);
}

UEdGraph* FBlueprintGraphWriteFacade::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	return FBlueprintGraphGenerationPipeline::FindGraphByName(Blueprint, GraphName);
}

TArray<TSharedPtr<FEngineFunctionItem>> FBlueprintGraphWriteFacade::GetAllBlueprintFunctions()
{
	return FBlueprintGraphNodeUtility::GetAllBlueprintFunctions();
}

TArray<FBlueprintGeneratorDiagnostic> FBlueprintGraphWriteFacade::ApplyDefaultValues(
	UK2Node* TargetNode,
	const TMap<FString, FString>& DefaultValues,
	const FString& NodeId)
{
	return FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(TargetNode, DefaultValues, NodeId);
}

UEdGraphPin* FBlueprintGraphWriteFacade::FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName)
{
	return FBlueprintGraphNodeUtility::FindPinByAlias(TargetNode, RequestedPinName);
}
