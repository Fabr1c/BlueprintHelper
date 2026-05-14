#include "Systems/ToolClusters/GraphWrite/NodeHandlers/MacroInstanceNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FMacroInstanceNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::MacroInstance;
}

UK2Node* FMacroInstanceNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	return FBlueprintGraphWriteFacade::SpawnMacroNode(TargetGraph, NodeData, OutError);
}
