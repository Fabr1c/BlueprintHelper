#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SelfNodeHandler.h"

#include "K2Node_Self.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FSelfNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Self;
}

UK2Node* FSelfNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Self 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(TargetGraph);
	TargetGraph->AddNode(SelfNode, true, false);
	SelfNode->CreateNewGuid();
	SelfNode->PostPlacedNewNode();
	SelfNode->NodePosX = static_cast<int32>(NodeData.X);
	SelfNode->NodePosY = static_cast<int32>(NodeData.Y);
	SelfNode->AllocateDefaultPins();

	return SelfNode;
}
