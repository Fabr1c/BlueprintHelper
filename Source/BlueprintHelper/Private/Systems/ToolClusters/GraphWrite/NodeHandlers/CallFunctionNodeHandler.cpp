#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.h"

#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

bool FCallFunctionNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CallFunction;
}

UK2Node* FCallFunctionNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UFunction* TargetFunction = TextToBlueprintGenerator::FindFunctionByName(NodeData.FunctionName);
	if (!TargetFunction)
	{
		OutError = FString::Printf(TEXT("未找到蓝图函数：%s"), *NodeData.FunctionName);
		return nullptr;
	}

	return TextToBlueprintGenerator::SpawnFunctionNode(TargetGraph, TargetFunction, NodeData);
}
