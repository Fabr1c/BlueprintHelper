#include "NodeHandlers/CommutativeAssociativeBinaryOperatorNodeHandler.h"

#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "TextToBlueprintGenerator.h"

bool FCommutativeAssociativeBinaryOperatorNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CommutativeAssociativeBinaryOperator;
}

UK2Node* FCommutativeAssociativeBinaryOperatorNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("CommutativeAssociativeBinaryOperator 节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.FunctionName.IsEmpty())
	{
		OutError = TEXT("CommutativeAssociativeBinaryOperator 节点生成失败：function_name 为空。");
		return nullptr;
	}

	UFunction* TargetFunction = TextToBlueprintGenerator::FindFunctionByName(NodeData.FunctionName);
	if (!TargetFunction)
	{
		OutError = FString::Printf(TEXT("CommutativeAssociativeBinaryOperator 节点生成失败：未找到函数 '%s'。"), *NodeData.FunctionName);
		return nullptr;
	}

	UK2Node_CommutativeAssociativeBinaryOperator* NewNode = NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(TargetGraph);
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
