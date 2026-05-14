#include "Systems/ToolClusters/GraphWrite/NodeHandlers/VariableSetNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

bool FVariableSetNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::VariableSet;
}

UK2Node* FVariableSetNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	FBlueprintHelperNodeFragment Fragment;
	if (!FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(TargetGraph, NodeData, Fragment, OutError))
	{
		return nullptr;
	}

	return Fragment.PrimaryNode;
}
