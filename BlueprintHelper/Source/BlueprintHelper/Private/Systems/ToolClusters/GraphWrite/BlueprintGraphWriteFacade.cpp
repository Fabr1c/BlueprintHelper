#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"

UFunction* FBlueprintGraphWriteFacade::FindFunctionByName(const FString& FuncName)
{
	return FBlueprintGraphNodeUtility::FindFunctionByName(FuncName);
}

FBlueprintHelperCallFunctionResolveResult FBlueprintGraphWriteFacade::ResolveFunctionForGraph(
	UEdGraph* TargetGraph,
	const FString& FunctionQuery,
	const TMap<FString, FString>& DefaultValues)
{
	return FBlueprintGraphNodeUtility::ResolveFunctionForGraph(TargetGraph, FunctionQuery, DefaultValues);
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

UK2Node_CallFunction* FBlueprintGraphWriteFacade::SpawnFunctionNode(
	UEdGraph* TargetGraph,
	UFunction* TargetFunction,
	const FParsedNode& NodeData)
{
	return FBlueprintGraphNodeSpawner::SpawnFunctionNode(TargetGraph, TargetFunction, NodeData);
}

UK2Node* FBlueprintGraphWriteFacade::SpawnVariableGetNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FString& OutErrorMessage)
{
	return FBlueprintGraphNodeSpawner::SpawnVariableGetNode(TargetGraph, NodeData, OutErrorMessage);
}

UK2Node* FBlueprintGraphWriteFacade::SpawnVariableSetNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FString& OutErrorMessage)
{
	return FBlueprintGraphNodeSpawner::SpawnVariableSetNode(TargetGraph, NodeData, OutErrorMessage);
}

UK2Node* FBlueprintGraphWriteFacade::SpawnMacroNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FString& OutErrorMessage)
{
	return FBlueprintGraphNodeSpawner::SpawnMacroNode(TargetGraph, NodeData, OutErrorMessage);
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
