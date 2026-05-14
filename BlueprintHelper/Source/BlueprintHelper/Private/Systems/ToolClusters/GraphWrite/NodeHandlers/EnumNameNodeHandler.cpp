#include "Systems/ToolClusters/GraphWrite/NodeHandlers/EnumNameNodeHandler.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "EdGraph/EdGraph.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FEnumNameNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::GetEnumeratorName
		|| NodeType == EParsedBlueprintNodeType::GetEnumeratorNameAsString;
}

UK2Node* FEnumNameNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("EnumName 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node* ResultNode = nullptr;

	if (NodeData.NodeType == EParsedBlueprintNodeType::GetEnumeratorNameAsString)
	{
		UK2Node_GetEnumeratorNameAsString* NameAsStringNode = NewObject<UK2Node_GetEnumeratorNameAsString>(TargetGraph);
		TargetGraph->AddNode(NameAsStringNode, true, false);
		NameAsStringNode->CreateNewGuid();
		NameAsStringNode->PostPlacedNewNode();
		NameAsStringNode->NodePosX = static_cast<int32>(NodeData.X);
		NameAsStringNode->NodePosY = static_cast<int32>(NodeData.Y);
		NameAsStringNode->AllocateDefaultPins();
		ResultNode = NameAsStringNode;
	}
	else
	{
		UK2Node_GetEnumeratorName* NameNode = NewObject<UK2Node_GetEnumeratorName>(TargetGraph);
		TargetGraph->AddNode(NameNode, true, false);
		NameNode->CreateNewGuid();
		NameNode->PostPlacedNewNode();
		NameNode->NodePosX = static_cast<int32>(NodeData.X);
		NameNode->NodePosY = static_cast<int32>(NodeData.Y);
		NameNode->AllocateDefaultPins();
		ResultNode = NameNode;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(ResultNode, NodeData.DefaultValues);
	return ResultNode;
}
