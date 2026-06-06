#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBuildService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "HAL/PlatformTime.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

#include "Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.h"

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
	UEdGraph* TargetGraph,
	const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	const FBlueprintGraphWriteConnectivityValidationInput* ConnectivityInput)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("Generation failed.");

	if (!TargetGraph)
	{
		Result.Message = TEXT("Target graph is invalid.");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty())
	{
		Result.Message = TEXT("JSON text is empty.");
		return Result;
	}

	if (!TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("GraphWrite input must be a JSON object.");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON parse failed: %s"), *Reader->GetErrorMessage());
		return Result;
	}

	if (JsonObject->HasField(TEXT("graphs")))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("Unable to resolve Blueprint for multi-graph SemanticIR input.");
			return Result;
		}

		return FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(Blueprint, TrimmedJsonString, OutUnresolvedNodes);
	}

	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& OpValue : *BlueprintOpsArray)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName.IsEmpty())
			{
				continue;
			}

			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
			UnresolvedItem->Reason = FString::Printf(TEXT("Blueprint operation '%s' is unsupported because the legacy GraphWrite operation path has been removed."), *OpName);
			OutUnresolvedNodes.Add(UnresolvedItem);
		}
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return UGraphWritePipelineUtils::GenerateSemanticGraphFromJsonObject(
			TargetGraph,
			JsonObject,
			OutUnresolvedNodes,
			ConnectivityInput);
	}

	Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. nodes/links node creation is disabled.");
	return Result;
}

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateSemanticGraphForGraph(
	UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	const FBlueprintGraphWriteConnectivityValidationInput* ConnectivityInput)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("Generation failed.");

	if (!TargetGraph || !GraphJsonObject.IsValid())
	{
		Result.Message = TEXT("Target graph or graph JSON object is invalid.");
		return Result;
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (GraphJsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return UGraphWritePipelineUtils::GenerateSemanticGraphFromJsonObject(
			TargetGraph,
			GraphJsonObject,
			OutUnresolvedNodes,
			ConnectivityInput);
	}

	Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. nodes/links node creation is disabled.");
	return Result;
}

UEdGraph* FBlueprintGraphGenerationPipeline::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint || GraphName.IsEmpty())
	{
		return nullptr;
	}

	// EventGraph閿涙碍鎮崇槐?UbergraphPages
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	// 娑旂喎婀?UbergraphPages 娑擃厽瀵滅划鍓р€橀崥宥囆為幖婊呭偍
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 閸戣姤鏆熼崶?
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 鐎瑰繐娴?
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 婵梹澧粵鎯ф倳閸?
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	return nullptr;
}
