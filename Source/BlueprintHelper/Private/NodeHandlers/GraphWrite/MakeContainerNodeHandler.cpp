#include "NodeHandlers/GraphWrite/MakeContainerNodeHandler.h"

#include "K2Node_MakeArray.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeMap.h"
#include "GraphWrite/TextToBlueprintGenerator.h"

bool FMakeContainerNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::MakeArray
		|| NodeType == EParsedBlueprintNodeType::MakeSet
		|| NodeType == EParsedBlueprintNodeType::MakeMap;
}

UK2Node* FMakeContainerNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("MakeContainer 节点生成失败：目标图表无效。");
		return nullptr;
	}

	switch (NodeData.NodeType)
	{
	case EParsedBlueprintNodeType::MakeArray:
		return SpawnMakeArray(TargetGraph, NodeData, OutError);
	case EParsedBlueprintNodeType::MakeSet:
		return SpawnMakeSet(TargetGraph, NodeData, OutError);
	case EParsedBlueprintNodeType::MakeMap:
		return SpawnMakeMap(TargetGraph, NodeData, OutError);
	default:
		OutError = TEXT("MakeContainer 节点生成失败：意外的节点类型。");
		return nullptr;
	}
}

UK2Node* FMakeContainerNodeHandler::SpawnMakeArray(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UK2Node_MakeArray* ArrayNode = NewObject<UK2Node_MakeArray>(TargetGraph);
	TargetGraph->AddNode(ArrayNode, true, false);
	ArrayNode->CreateNewGuid();

	const int32 NumInputs = FMath::Max(NodeData.ContainerReference.NumInputs, 1);
	ArrayNode->NumInputs = NumInputs;

	ArrayNode->AllocateDefaultPins();
	ArrayNode->PostPlacedNewNode();

	ArrayNode->NodePosX = static_cast<int32>(NodeData.X);
	ArrayNode->NodePosY = static_cast<int32>(NodeData.Y);

	TextToBlueprintGenerator::ApplyDefaultValues(ArrayNode, NodeData.DefaultValues);
	return ArrayNode;
}

UK2Node* FMakeContainerNodeHandler::SpawnMakeSet(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UK2Node_MakeSet* SetNode = NewObject<UK2Node_MakeSet>(TargetGraph);
	TargetGraph->AddNode(SetNode, true, false);
	SetNode->CreateNewGuid();

	const int32 NumInputs = FMath::Max(NodeData.ContainerReference.NumInputs, 1);
	SetNode->NumInputs = NumInputs;

	SetNode->AllocateDefaultPins();
	SetNode->PostPlacedNewNode();

	SetNode->NodePosX = static_cast<int32>(NodeData.X);
	SetNode->NodePosY = static_cast<int32>(NodeData.Y);

	TextToBlueprintGenerator::ApplyDefaultValues(SetNode, NodeData.DefaultValues);
	return SetNode;
}

UK2Node* FMakeContainerNodeHandler::SpawnMakeMap(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UK2Node_MakeMap* MapNode = NewObject<UK2Node_MakeMap>(TargetGraph);
	TargetGraph->AddNode(MapNode, true, false);
	MapNode->CreateNewGuid();

	const int32 NumPairs = FMath::Max(NodeData.ContainerReference.NumPairs, 1);
	MapNode->NumInputs = NumPairs;

	MapNode->AllocateDefaultPins();
	MapNode->PostPlacedNewNode();

	MapNode->NodePosX = static_cast<int32>(NodeData.X);
	MapNode->NodePosY = static_cast<int32>(NodeData.Y);

	TextToBlueprintGenerator::ApplyDefaultValues(MapNode, NodeData.DefaultValues);
	return MapNode;
}
