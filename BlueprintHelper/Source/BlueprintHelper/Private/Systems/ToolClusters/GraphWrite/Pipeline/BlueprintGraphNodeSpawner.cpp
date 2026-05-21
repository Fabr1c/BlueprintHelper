#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
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
#include "BlueprintNodeSpawner.h"
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

	UBlueprintNodeSpawner* MacroSpawner = UBlueprintNodeSpawner::Create(UK2Node_MacroInstance::StaticClass());
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = NodeData.Id;
	SpawnOptions.DefaultValues = NodeData.DefaultValues;
	SpawnOptions.bReconstructAfterSpawn = false;
	SpawnOptions.NodeConfigurationHook = [MacroGraph](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext& Context, FString& HookError)
	{
		UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(&SpawnedNode);
		if (!MacroNode)
		{
			HookError = TEXT("macro node generation failed: spawned node is not UK2Node_MacroInstance.");
			return false;
		}

		MacroNode->SetMacroGraph(MacroGraph);
		if (MacroNode->Pins.Num() == 0)
		{
			MacroNode->AllocateDefaultPins();
		}
		if (Context.TargetGraph && Context.TargetGraph->GetSchema())
		{
			Context.TargetGraph->GetSchema()->ReconstructNode(*MacroNode);
		}
		return true;
	};

	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
		TargetGraph,
		MacroSpawner,
		TEXT("macro_instance_node_spawner"),
		FVector2D(NodeData.X, NodeData.Y),
		SpawnOptions,
		OutErrorMessage);
	return Cast<UK2Node_MacroInstance>(SpawnedNode);
}

