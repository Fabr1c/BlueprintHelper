#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"

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

FBlueprintGenerateResult FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(
	UBlueprint* Blueprint, const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。");

	if (!Blueprint)
	{
		Result.Message = TEXT("蓝图对象无效。");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty() || !TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("JSON 文本无效。");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON 解析失败：%s"), *Reader->GetErrorMessage());
		return Result;
	}

	// === 蓝图级操作 ===
	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
		for (int32 OpIndex = 0; OpIndex < BlueprintOpsArray->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject = (*BlueprintOpsArray)[OpIndex].IsValid()
				? (*BlueprintOpsArray)[OpIndex]->AsObject()
				: nullptr;
			FString OpName;
			if (OpObject.IsValid())
			{
				OpObject->TryGetStringField(TEXT("op"), OpName);
			}

			if (OpName.IsEmpty())
			{
				OpName = FString::Printf(TEXT("<invalid:%d>"), OpIndex);
			}

			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
			UnresolvedItem->Reason = TEXT("GraphWrite only accepts logic_spec/SemanticIR. blueprint_operations is disabled.");
			OutUnresolvedNodes.Add(UnresolvedItem);
		}

		Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
		Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. blueprint_operations is disabled.");
		return Result;
	}

	// === 多图模式（graphs 数组） ===
	int32 TotalGenerated = 0;
	int32 TotalRequestedDefaultValues = 0;
	int32 TotalAppliedDefaultValues = 0;
	int32 TotalRequestedPinTypes = 0;
	int32 TotalResolvedPinTypes = 0;
	int32 TotalRequestedConnections = 0;
	int32 TotalCreatedConnections = 0;
	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("graphs"), GraphsArray) && GraphsArray && GraphsArray->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& GraphValue : *GraphsArray)
		{
			const TSharedPtr<FJsonObject> GraphObject = GraphValue->AsObject();
			if (!GraphObject.IsValid())
			{
				continue;
			}

			FString GraphName;
			GraphObject->TryGetStringField(TEXT("graph"), GraphName);
			if (GraphName.IsEmpty())
			{
				continue;
			}

			UEdGraph* TargetGraph = FBlueprintGraphGenerationPipeline::FindGraphByName(Blueprint, GraphName);
			if (!TargetGraph)
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("Graph: %s"), *GraphName);
				UnresolvedItem->Reason = FString::Printf(TEXT("Target graph '%s' was not found. Create it through Blueprint structure services before GraphWrite."), *GraphName);
				OutUnresolvedNodes.Add(UnresolvedItem);
				continue;
			}

			FBlueprintGenerateResult GraphResult = FBlueprintGraphGenerationPipeline::GenerateNodesAndLinksForGraph(TargetGraph, GraphObject, OutUnresolvedNodes);
			TotalGenerated += GraphResult.GeneratedNodeCount;
			TotalRequestedDefaultValues += GraphResult.RequestedDefaultValueCount;
			TotalAppliedDefaultValues += GraphResult.AppliedDefaultValueCount;
			Result.DefaultValueDiagnostics.Append(GraphResult.DefaultValueDiagnostics);
			TotalRequestedPinTypes += GraphResult.RequestedPinTypeCount;
			TotalResolvedPinTypes += GraphResult.ResolvedPinTypeCount;
			Result.PinTypeDiagnostics.Append(GraphResult.PinTypeDiagnostics);
			TotalRequestedConnections += GraphResult.RequestedConnectionCount;
			TotalCreatedConnections += GraphResult.CreatedConnectionCount;
			Result.ConnectionDiagnostics.Append(GraphResult.ConnectionDiagnostics);
		}
	}
	else if (JsonObject->HasField(TEXT("nodes")) || JsonObject->HasField(TEXT("links")))
	{
		Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. nodes/links node creation is disabled.");
		return Result;
	}

	Result.bSucceed = TotalGenerated > 0;
	Result.GeneratedNodeCount = TotalGenerated;
	Result.RequestedDefaultValueCount = TotalRequestedDefaultValues;
	Result.AppliedDefaultValueCount = TotalAppliedDefaultValues;
	Result.RequestedPinTypeCount = TotalRequestedPinTypes;
	Result.ResolvedPinTypeCount = TotalResolvedPinTypes;
	Result.RequestedConnectionCount = TotalRequestedConnections;
	Result.CreatedConnectionCount = TotalCreatedConnections;
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("多图生成完成：成功 %d 个节点，未匹配 %d 个。"), Result.GeneratedNodeCount, Result.UnresolvedNodeCount);
	}
	else if (Result.UnresolvedNodeCount > 0)
	{
		Result.Message = FString::Printf(TEXT("未生成任何节点：共有 %d 个未匹配项。"), Result.UnresolvedNodeCount);
	}
	else
	{
		Result.Message = TEXT("未生成任何节点。");
	}
	return Result;
}
