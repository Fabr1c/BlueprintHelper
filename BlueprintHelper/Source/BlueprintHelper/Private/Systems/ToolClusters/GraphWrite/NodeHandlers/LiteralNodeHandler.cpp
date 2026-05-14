#include "Systems/ToolClusters/GraphWrite/NodeHandlers/LiteralNodeHandler.h"
#include "K2Node_Literal.h"
#include "EdGraph/EdGraph.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "UObject/UObjectGlobals.h"

bool FLiteralNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Literal;
}

UK2Node* FLiteralNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Literal 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_Literal* LiteralNode = NewObject<UK2Node_Literal>(TargetGraph);
	TargetGraph->AddNode(LiteralNode, true, false);
	LiteralNode->CreateNewGuid();
	LiteralNode->PostPlacedNewNode();
	LiteralNode->NodePosX = static_cast<int32>(NodeData.X);
	LiteralNode->NodePosY = static_cast<int32>(NodeData.Y);

	if (!NodeData.LiteralReference.ObjectPath.IsEmpty())
	{
		UObject* ReferencedObject = StaticFindObject(UObject::StaticClass(), nullptr, *NodeData.LiteralReference.ObjectPath);
		if (!ReferencedObject)
		{
			ReferencedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *NodeData.LiteralReference.ObjectPath);
		}
		if (ReferencedObject)
		{
			LiteralNode->SetObjectRef(ReferencedObject);
		}
		else
		{
			OutError = FString::Printf(TEXT("Literal 节点：无法加载对象引。'%s'。"), *NodeData.LiteralReference.ObjectPath);
			return nullptr;
		}
	}

	LiteralNode->AllocateDefaultPins();
	return LiteralNode;
}
