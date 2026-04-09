#include "NodeHandlers/VariableSetNodeHandler.h"
#include "TextToBlueprintGenerator.h"

bool FVariableSetNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::VariableSet;
}

UK2Node* FVariableSetNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (NodeData.VariableReference.IsLocalVariable() && NodeData.VariableReference.bEnsureExists)
	{
		FParsedLocalVariableDeclaration LocalDeclaration;
		LocalDeclaration.Name = NodeData.VariableReference.VariableName;
		LocalDeclaration.PinType = NodeData.VariableReference.PinType;
		LocalDeclaration.DefaultValue = NodeData.VariableReference.DefaultValue;
		LocalDeclaration.bEnsureExists = true;
		TextToBlueprintGenerator::EnsureLocalVariableExists(TargetGraph, LocalDeclaration, OutError);
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
	}

	return TextToBlueprintGenerator::SpawnVariableSetNode(TargetGraph, NodeData, OutError);
}
