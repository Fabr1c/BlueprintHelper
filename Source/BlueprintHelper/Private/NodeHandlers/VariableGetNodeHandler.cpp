#include "NodeHandlers/VariableGetNodeHandler.h"
#include "TextToBlueprintGenerator.h"

bool FVariableGetNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::VariableGet;
}

UK2Node* FVariableGetNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
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

	return TextToBlueprintGenerator::SpawnVariableGetNode(TargetGraph, NodeData, OutError);
}
