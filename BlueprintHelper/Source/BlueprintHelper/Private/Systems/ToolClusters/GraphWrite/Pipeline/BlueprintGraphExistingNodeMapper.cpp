#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
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
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

void FBlueprintGraphExistingNodeMapper::MapExistingNodeRefs(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject, TMap<FString, UK2Node*>& IdToSpawnedNode)
{
	if (!TargetGraph || !GraphJsonObject.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (!GraphJsonObject->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray) || !ExistingRefsArray)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& RefValue : *ExistingRefsArray)
	{
		const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
		if (!RefObject.IsValid())
		{
			continue;
		}
		FString RefId;
		RefObject->TryGetStringField(TEXT("id"), RefId);
		if (RefId.IsEmpty())
		{
			continue;
		}
		FString MatchTitle;
		RefObject->TryGetStringField(TEXT("node_title"), MatchTitle);
		FString MatchGuid;
		RefObject->TryGetStringField(TEXT("node_guid"), MatchGuid);
		for (UEdGraphNode* RefCandidate : TargetGraph->Nodes)
		{
			UK2Node* K2Existing = Cast<UK2Node>(RefCandidate);
			if (!K2Existing || IdToSpawnedNode.FindKey(K2Existing))
			{
				continue;
			}
			bool bMatched = false;
			if (!MatchGuid.IsEmpty())
			{
				bMatched = RefCandidate->NodeGuid.ToString(EGuidFormats::Digits) == MatchGuid;
			}
			else if (!MatchTitle.IsEmpty())
			{
				const FString Title = RefCandidate->GetNodeTitle(ENodeTitleType::ListView).ToString();
				bMatched = Title.Contains(MatchTitle);
			}
			if (bMatched)
			{
				IdToSpawnedNode.Add(RefId, K2Existing);
				break;
			}
		}
	}
}
