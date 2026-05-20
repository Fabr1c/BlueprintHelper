#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SequenceNodeHandler.h"

#include "K2Node_ExecutionSequence.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"

bool FSequenceNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Sequence;
}

UK2Node* FSequenceNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Sequence 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_ExecutionSequence* SequenceNode =
		FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_ExecutionSequence>(
			TargetGraph,
			FVector2D(NodeData.X, NodeData.Y));

	// Sequence 默认。2 个输出引脚。如。JSON 指定。num_outputs，添加额外引脚。
	const FString* NumOutputsValue = NodeData.DefaultValues.Find(TEXT("num_outputs"));
	if (NumOutputsValue)
	{
		const int32 DesiredOutputs = FCString::Atoi(**NumOutputsValue);
		const int32 CurrentOutputs = 2;
		for (int32 Index = CurrentOutputs; Index < DesiredOutputs; ++Index)
		{
			SequenceNode->AddInputPin();
		}
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(SequenceNode, NodeData.DefaultValues);
	return SequenceNode;
}
