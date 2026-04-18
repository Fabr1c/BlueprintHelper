#include "NodeHandlers/GetArrayItemNodeHandler.h"

#include "K2Node_GetArrayItem.h"
#include "TextToBlueprintGenerator.h"
#include "EdGraphSchema_K2.h"

bool FGetArrayItemNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::GetArrayItem;
}

UK2Node* FGetArrayItemNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("GetArrayItem 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_GetArrayItem* ArrayItemNode = NewObject<UK2Node_GetArrayItem>(TargetGraph);
	TargetGraph->AddNode(ArrayItemNode, true, false);
	ArrayItemNode->CreateNewGuid();
	ArrayItemNode->PostPlacedNewNode();
	ArrayItemNode->NodePosX = static_cast<int32>(NodeData.X);
	ArrayItemNode->NodePosY = static_cast<int32>(NodeData.Y);
	ArrayItemNode->AllocateDefaultPins();

	TextToBlueprintGenerator::ApplyDefaultValues(ArrayItemNode, NodeData.DefaultValues);

	return ArrayItemNode;
}
