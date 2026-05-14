#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

bool FCallFunctionNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CallFunction;
}

UK2Node* FCallFunctionNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	FBlueprintHelperNodeFragment Fragment;
	if (!FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(TargetGraph, NodeData, Fragment, OutError))
	{
		return nullptr;
	}

	return Fragment.PrimaryNode;
}
