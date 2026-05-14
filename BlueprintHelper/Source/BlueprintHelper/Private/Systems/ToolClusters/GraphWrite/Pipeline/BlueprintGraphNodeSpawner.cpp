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

UK2Node* FBlueprintGraphNodeSpawner::SpawnVariableGetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.VariableReference.VariableName.IsEmpty())
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：变量名为空。");
		return nullptr;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：K2 Schema 无效。");
		return nullptr;
	}

	UStruct* VariableSource = FBlueprintGraphLocalVariableService::ResolveVariableSource(TargetGraph, NodeData.VariableReference, OutErrorMessage);
	if (!OutErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	if (UK2Node_VariableGet* VariableNode = Schema->SpawnVariableGetNode(FVector2D(NodeData.X, NodeData.Y), TargetGraph, *NodeData.VariableReference.VariableName, VariableSource))
	{
		FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(VariableNode, NodeData.DefaultValues);
		return VariableNode;
	}

	OutErrorMessage = FString::Printf(TEXT("无法生成变量读取节点：%s"), *NodeData.VariableReference.VariableName);
	return nullptr;
}

UK2Node* FBlueprintGraphNodeSpawner::SpawnVariableSetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.VariableReference.VariableName.IsEmpty())
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：变量名为空。");
		return nullptr;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：K2 Schema 无效。");
		return nullptr;
	}

	UStruct* VariableSource = FBlueprintGraphLocalVariableService::ResolveVariableSource(TargetGraph, NodeData.VariableReference, OutErrorMessage);
	if (!OutErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	if (UK2Node_VariableSet* VariableNode = Schema->SpawnVariableSetNode(FVector2D(NodeData.X, NodeData.Y), TargetGraph, *NodeData.VariableReference.VariableName, VariableSource))
	{
		FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(VariableNode, NodeData.DefaultValues);
		return VariableNode;
	}

	OutErrorMessage = FString::Printf(TEXT("无法生成变量写入节点：%s"), *NodeData.VariableReference.VariableName);
	return nullptr;
}

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

UK2Node_CallFunction* FBlueprintGraphNodeSpawner::SpawnFunctionNode(UEdGraph* TargetGraph, UFunction* TargetFunction, const FParsedNode& NodeData)
{
	if (!TargetGraph || !TargetFunction)
	{
		return nullptr;
	}

	UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(TargetGraph);
	TargetGraph->AddNode(CallFunctionNode, true, false);
	CallFunctionNode->CreateNewGuid();
	CallFunctionNode->PostPlacedNewNode();
	CallFunctionNode->SetFromFunction(TargetFunction);
	CallFunctionNode->NodePosX = static_cast<int32>(NodeData.X);
	CallFunctionNode->NodePosY = static_cast<int32>(NodeData.Y);
	CallFunctionNode->AllocateDefaultPins();
	TargetGraph->GetSchema()->ReconstructNode(*CallFunctionNode);
	FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(CallFunctionNode, NodeData.DefaultValues, NodeData.Id);
	return CallFunctionNode;
}
