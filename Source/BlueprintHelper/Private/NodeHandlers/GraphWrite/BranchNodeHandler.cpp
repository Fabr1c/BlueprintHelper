#include "NodeHandlers/GraphWrite/BranchNodeHandler.h"

#include "K2Node_IfThenElse.h"
#include "GraphWrite/TextToBlueprintGenerator.h"

bool FBranchNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Branch;
}

UK2Node* FBranchNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Branch 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(TargetGraph);
	TargetGraph->AddNode(BranchNode, true, false);
	BranchNode->CreateNewGuid();
	BranchNode->PostPlacedNewNode();
	BranchNode->NodePosX = static_cast<int32>(NodeData.X);
	BranchNode->NodePosY = static_cast<int32>(NodeData.Y);
	BranchNode->AllocateDefaultPins();
	TextToBlueprintGenerator::ApplyDefaultValues(BranchNode, NodeData.DefaultValues);
	return BranchNode;
}
