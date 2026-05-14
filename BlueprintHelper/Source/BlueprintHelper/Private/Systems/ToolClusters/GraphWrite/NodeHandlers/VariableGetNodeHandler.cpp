#include "Systems/ToolClusters/GraphWrite/NodeHandlers/VariableGetNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

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
		FBlueprintGraphWriteFacade::EnsureLocalVariableExists(TargetGraph, LocalDeclaration, OutError);
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
	}

	return FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, NodeData, OutError);
}
