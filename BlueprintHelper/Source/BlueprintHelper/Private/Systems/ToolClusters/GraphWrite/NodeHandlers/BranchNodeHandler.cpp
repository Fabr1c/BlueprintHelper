#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BranchNodeHandler.h"

#include "K2Node_IfThenElse.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"

bool FBranchNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Branch;
}

UK2Node* FBranchNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Branch 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_IfThenElse* BranchNode =
		FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_IfThenElse>(
			TargetGraph,
			FVector2D(NodeData.X, NodeData.Y));
	FBlueprintGraphWriteFacade::ApplyDefaultValues(BranchNode, NodeData.DefaultValues);
	return BranchNode;
}
