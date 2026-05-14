#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/OperationHandlers/BlueprintOperationHandler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"


int32 FBlueprintGraphLinker::ConnectComposerExecChain(UEdGraph* TargetGraph, const TArray<FParsedLink>& ParsedLinks, const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics)
{
	const bool bHasExplicitExecLinks = ParsedLinks.ContainsByPredicate([](const FParsedLink& ParsedLink)
	{
		return ParsedLink.FromPin.Equals(TEXT("then"), ESearchCase::IgnoreCase)
			|| ParsedLink.ToPin.Equals(TEXT("execute"), ESearchCase::IgnoreCase);
	});
	if (bHasExplicitExecLinks || GeneratedFragments.Num() <= 1)
	{
		return 0;
	}
	const FBlueprintHelperGraphComposeResult ComposeResult = FBlueprintHelperGraphComposer::ConnectLinearExecChain(TargetGraph, GeneratedFragments);
	for (const FString& ComposeDiagnostic : ComposeResult.Diagnostics)
	{
		ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("composer_exec_connection_rejected"), TEXT("graph_composer"), TEXT("execute"), ComposeDiagnostic));
	}
	return ComposeResult.CreatedExecConnectionCount;
}

int32 FBlueprintGraphLinker::ConnectFragmentDataEdges(
	UEdGraph* TargetGraph,
	const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	const TArray<FBlueprintHelperGraphFragmentDataEdge>& DataEdges,
	TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics)
{
	if (GeneratedFragments.Num() == 0 || DataEdges.Num() == 0)
	{
		return 0;
	}

	const FBlueprintHelperGraphComposeResult ComposeResult =
		FBlueprintHelperGraphComposer::ConnectDataEdges(TargetGraph, GeneratedFragments, DataEdges);
	for (const FString& ComposeDiagnostic : ComposeResult.Diagnostics)
	{
		ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
			TEXT("composer_data_connection_rejected"),
			TEXT("graph_composer"),
			TEXT("data"),
			ComposeDiagnostic));
	}
	return ComposeResult.CreatedDataConnectionCount;
}

int32 FBlueprintGraphLinker::ConnectExplicitLinks(UEdGraph* TargetGraph, const TArray<FParsedLink>& ParsedLinks, const TMap<FString, UK2Node*>& IdToSpawnedNode, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 CreatedConnectionCount = 0;
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_connection_rejected"), ParsedLink.FromId, ParsedLink.FromPin, TEXT("K2 schema is invalid.")));
			continue;
		}
		UK2Node* const* FromNodePtr = IdToSpawnedNode.Find(ParsedLink.FromId);
		UK2Node* const* ToNodePtr = IdToSpawnedNode.Find(ParsedLink.ToId);
		if (!FromNodePtr || !*FromNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_node_not_found"), ParsedLink.FromId, ParsedLink.FromPin, FString::Printf(TEXT("Link source node not found: %s."), *ParsedLink.FromId)));
			continue;
		}
		if (!ToNodePtr || !*ToNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_node_not_found"), ParsedLink.ToId, ParsedLink.ToPin, FString::Printf(TEXT("Link target node not found: %s."), *ParsedLink.ToId)));
			continue;
		}
		UEdGraphPin* FromPin = FBlueprintGraphNodeUtility::FindPinByAlias(*FromNodePtr, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FBlueprintGraphNodeUtility::FindPinByAlias(*ToNodePtr, ParsedLink.ToPin);
		if (!FromPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_pin_not_found"), ParsedLink.FromId, ParsedLink.FromPin, FString::Printf(TEXT("Link source pin not found: %s.%s."), *ParsedLink.FromId, *ParsedLink.FromPin)));
			continue;
		}
		if (!ToPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_pin_not_found"), ParsedLink.ToId, ParsedLink.ToPin, FString::Printf(TEXT("Link target pin not found: %s.%s."), *ParsedLink.ToId, *ParsedLink.ToPin)));
			continue;
		}
		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++CreatedConnectionCount;
		}
		else
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(TEXT("link_connection_rejected"), ParsedLink.FromId, ParsedLink.FromPin, ConnectionResponse.Message.IsEmpty() ? FString::Printf(TEXT("Schema rejected link: %s.%s -> %s.%s."), *ParsedLink.FromId, *ParsedLink.FromPin, *ParsedLink.ToId, *ParsedLink.ToPin) : ConnectionResponse.Message.ToString()));
		}
	}
	return CreatedConnectionCount;
}
