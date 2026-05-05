#include "NodeHandlers/GraphWrite/MacroInstanceNodeHandler.h"
#include "GraphWrite/TextToBlueprintGenerator.h"

bool FMacroInstanceNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::MacroInstance;
}

UK2Node* FMacroInstanceNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	return TextToBlueprintGenerator::SpawnMacroNode(TargetGraph, NodeData, OutError);
}
