#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SelectNodeHandler.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

namespace
{
static void ApplyIndexPinType(UK2Node_Select* SelectNode, const FName& PinCategory)
{
	if (!SelectNode)
	{
		return;
	}

	UEdGraphPin* IndexPin = SelectNode->GetIndexPin();
	if (!IndexPin)
	{
		return;
	}

	IndexPin->PinType.PinCategory = PinCategory;
	IndexPin->PinType.PinSubCategory = NAME_None;
	IndexPin->PinType.PinSubCategoryObject = nullptr;
	SelectNode->ChangePinType(IndexPin);
}
}

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
	if (NodeData.SelectReference.EnumPath.IsEmpty())
	{
		ApplyIndexPinType(
			NewNode,
			NodeData.SelectReference.NumOptions <= 2
				? UEdGraphSchema_K2::PC_Boolean
				: UEdGraphSchema_K2::PC_Int);
	}

	// 按需添加额外选项引脚（默认有 2 个）
	const int32 ExtraOptions = FMath::Max(0, NodeData.SelectReference.NumOptions - 2);
	for (int32 i = 0; i < ExtraOptions; ++i)
	{
		NewNode->AddInputPin();
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	return NewNode;
}
