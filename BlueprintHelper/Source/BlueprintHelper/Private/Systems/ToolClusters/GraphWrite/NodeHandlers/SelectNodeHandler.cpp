#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SelectNodeHandler.h"

#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FSelectNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Select;
}

UK2Node* FSelectNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Select 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_Select* NewNode = NewObject<UK2Node_Select>(TargetGraph);
	TargetGraph->AddNode(NewNode, true, false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->NodePosX = static_cast<int32>(NodeData.X);
	NewNode->NodePosY = static_cast<int32>(NodeData.Y);

	// 如果指定了枚举路径，绑定枚举
	if (!NodeData.SelectReference.EnumPath.IsEmpty())
	{
		UEnum* Enum = FindObject<UEnum>(nullptr, *NodeData.SelectReference.EnumPath);
		if (!Enum)
		{
			Enum = LoadObject<UEnum>(nullptr, *NodeData.SelectReference.EnumPath);
		}
		if (Enum)
		{
			NewNode->SetEnum(Enum, true);
		}
	}

	NewNode->AllocateDefaultPins();

	// 按需添加额外选项引脚（默认有 2 个）
	const int32 ExtraOptions = FMath::Max(0, NodeData.SelectReference.NumOptions - 2);
	for (int32 i = 0; i < ExtraOptions; ++i)
	{
		NewNode->AddInputPin();
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	return NewNode;
}
