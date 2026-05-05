#include "NodeHandlers/GraphWrite/DynamicCastNodeHandler.h"

#include "K2Node_DynamicCast.h"
#include "GraphWrite/TextToBlueprintGenerator.h"
#include "UObject/UObjectGlobals.h"

bool FDynamicCastNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::DynamicCast;
}

UK2Node* FDynamicCastNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Cast 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& ClassPath = NodeData.CastReference.TargetClassPath;
	if (ClassPath.IsEmpty())
	{
		OutError = TEXT("Cast 节点生成失败：缺。cast.target_class_path。");
		return nullptr;
	}

	UClass* TargetClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!TargetClass)
	{
		TargetClass = LoadObject<UClass>(nullptr, *ClassPath);
	}
	if (!TargetClass)
	{
		OutError = FString::Printf(TEXT("Cast 节点生成失败：未找到目标。'%s'。"), *ClassPath);
		return nullptr;
	}

	UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(TargetGraph);
	TargetGraph->AddNode(CastNode, true, false);
	CastNode->CreateNewGuid();
	CastNode->PostPlacedNewNode();
	CastNode->TargetType = TargetClass;
	CastNode->NodePosX = static_cast<int32>(NodeData.X);
	CastNode->NodePosY = static_cast<int32>(NodeData.Y);
	CastNode->AllocateDefaultPins();

	TextToBlueprintGenerator::ApplyDefaultValues(CastNode, NodeData.DefaultValues);

	return CastNode;
}
