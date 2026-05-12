#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.h"

#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

bool FCallFunctionNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CallFunction;
}

UK2Node* FCallFunctionNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	const FBlueprintHelperCallFunctionResolveResult ResolveResult =
		TextToBlueprintGenerator::ResolveFunctionForGraph(TargetGraph, NodeData.FunctionName, NodeData.DefaultValues);

	if (!ResolveResult.IsResolved())
	{
		OutError = ResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *NodeData.FunctionName)
			: ResolveResult.Message;
		return nullptr;
	}

	UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);

	if (SpawnedNode)
	{
		TextToBlueprintGenerator::ApplyDefaultValues(SpawnedNode, NodeData.DefaultValues, NodeData.Id);
	}
	return SpawnedNode;
}
