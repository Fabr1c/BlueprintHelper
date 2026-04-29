#include "NodeHandlers/PromotableOperatorNodeHandler.h"

#include "K2Node_PromotableOperator.h"
#include "TextToBlueprintGenerator.h"

bool FPromotableOperatorNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::PromotableOperator;
}

UK2Node* FPromotableOperatorNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("PromotableOperator 节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.FunctionName.IsEmpty())
	{
		OutError = TEXT("PromotableOperator 节点生成失败：function_name 为空，请指定运算函数名（如 Add, Multiply 等）。");
		return nullptr;
	}

	// 查找运算函数
	UFunction* TargetFunction = TextToBlueprintGenerator::FindFunctionByName(NodeData.FunctionName);
	if (!TargetFunction)
	{
		OutError = FString::Printf(TEXT("PromotableOperator 节点生成失败：未找到运算函数 '%s'。"), *NodeData.FunctionName);
		return nullptr;
	}

	UK2Node_PromotableOperator* NewNode = NewObject<UK2Node_PromotableOperator>(TargetGraph);
	TargetGraph->AddNode(NewNode, true, false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->SetFromFunction(TargetFunction);
	NewNode->NodePosX = static_cast<int32>(NodeData.X);
	NewNode->NodePosY = static_cast<int32>(NodeData.Y);
	NewNode->AllocateDefaultPins();

	TextToBlueprintGenerator::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	return NewNode;
}
