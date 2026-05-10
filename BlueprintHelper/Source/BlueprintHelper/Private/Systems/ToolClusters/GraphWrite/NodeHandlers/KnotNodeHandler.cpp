#include "Systems/ToolClusters/GraphWrite/NodeHandlers/KnotNodeHandler.h"
#include "K2Node_Knot.h"
#include "EdGraph/EdGraph.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

bool FKnotNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Knot;
}

UK2Node* FKnotNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Knot 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(TargetGraph);
	TargetGraph->AddNode(KnotNode, true, false);
	KnotNode->CreateNewGuid();
	KnotNode->PostPlacedNewNode();
	KnotNode->NodePosX = static_cast<int32>(NodeData.X);
	KnotNode->NodePosY = static_cast<int32>(NodeData.Y);
	KnotNode->AllocateDefaultPins();

	return KnotNode;
}
