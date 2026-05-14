#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CreateDelegateNodeHandler.h"

#include "K2Node_CreateDelegate.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FCreateDelegateNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CreateDelegate;
}

UK2Node* FCreateDelegateNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("CreateDelegate 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_CreateDelegate* DelegateNode = NewObject<UK2Node_CreateDelegate>(TargetGraph);
	TargetGraph->AddNode(DelegateNode, true, false);
	DelegateNode->CreateNewGuid();
	DelegateNode->PostPlacedNewNode();
	DelegateNode->NodePosX = static_cast<int32>(NodeData.X);
	DelegateNode->NodePosY = static_cast<int32>(NodeData.Y);
	DelegateNode->AllocateDefaultPins();

	const FString& FunctionName = NodeData.DelegateReference.FunctionName;
	if (!FunctionName.IsEmpty())
	{
		DelegateNode->SetFunction(FName(*FunctionName));
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(DelegateNode, NodeData.DefaultValues);
	return DelegateNode;
}
