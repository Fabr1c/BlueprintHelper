#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"

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
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

UK2Node* FBlueprintGraphNodeSpawner::SpawnMacroNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("宏节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.MacroReference.MacroName.IsEmpty())
	{
		OutErrorMessage = TEXT("宏节点生成失败：缺少 macro.name，标准宏请提供真实宏名，例如 ForLoop。");
		return nullptr;
	}

	UEdGraph* MacroGraph = FBlueprintGraphNodeUtility::ResolveMacroGraph(NodeData.MacroReference, OutErrorMessage);
	if (!MacroGraph)
	{
		return nullptr;
	}

	UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(TargetGraph);
	TargetGraph->AddNode(MacroNode, true, false);
	MacroNode->CreateNewGuid();
	MacroNode->SetMacroGraph(MacroGraph);
	MacroNode->PostPlacedNewNode();
	MacroNode->NodePosX = static_cast<int32>(NodeData.X);
	MacroNode->NodePosY = static_cast<int32>(NodeData.Y);
	MacroNode->AllocateDefaultPins();
	TargetGraph->GetSchema()->ReconstructNode(*MacroNode);
	FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(MacroNode, NodeData.DefaultValues, NodeData.Id);
	return MacroNode;
}

