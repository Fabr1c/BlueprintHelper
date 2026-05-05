#include "NodeHandlers/GraphWrite/SequenceNodeHandler.h"

#include "K2Node_ExecutionSequence.h"
#include "GraphWrite/TextToBlueprintGenerator.h"

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

	UK2Node_ExecutionSequence* SequenceNode = NewObject<UK2Node_ExecutionSequence>(TargetGraph);
	TargetGraph->AddNode(SequenceNode, true, false);
	SequenceNode->CreateNewGuid();
	SequenceNode->PostPlacedNewNode();
	SequenceNode->NodePosX = static_cast<int32>(NodeData.X);
	SequenceNode->NodePosY = static_cast<int32>(NodeData.Y);
	SequenceNode->AllocateDefaultPins();

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

	TextToBlueprintGenerator::ApplyDefaultValues(SequenceNode, NodeData.DefaultValues);
	return SequenceNode;
}
