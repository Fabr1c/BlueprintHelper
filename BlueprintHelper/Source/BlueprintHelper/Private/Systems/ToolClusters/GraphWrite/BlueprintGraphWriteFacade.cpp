#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"

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

FBlueprintGenerateResult FBlueprintGraphWriteFacade::GenerateBlueprintFromJson(
	UEdGraph* TargetGraph,
	const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	return FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(TargetGraph, JsonString, OutUnresolvedNodes);
}

FBlueprintGenerateResult FBlueprintGraphWriteFacade::GenerateMultiGraphFromJson(
	UBlueprint* Blueprint,
	const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	return FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(Blueprint, JsonString, OutUnresolvedNodes);
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

bool FBlueprintGraphWriteFacade::EnsureLocalVariableExists(
	UEdGraph* TargetGraph,
	const FParsedLocalVariableDeclaration& Declaration,
	FString& OutErrorMessage)
{
	return FBlueprintGraphLocalVariableService::EnsureLocalVariableExists(TargetGraph, Declaration, OutErrorMessage);
}

bool FBlueprintGraphWriteFacade::ConvertToEdGraphPinType(
	const FParsedPinType& InPinType,
	FEdGraphPinType& OutPinType,
	FString& OutErrorMessage)
{
	return FBlueprintGraphLocalVariableService::ConvertToEdGraphPinType(InPinType, OutPinType, OutErrorMessage);
}

UEdGraphPin* FBlueprintGraphWriteFacade::FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName)
{
	return FBlueprintGraphNodeUtility::FindPinByAlias(TargetNode, RequestedPinName);
}
