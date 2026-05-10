#include "Systems/ToolClusters/GraphWrite/NodeHandlers/FormatTextNodeHandler.h"

#include "K2Node_FormatText.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"
#include "EdGraphSchema_K2.h"

bool FFormatTextNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::FormatText;
}

UK2Node* FFormatTextNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("FormatText 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_FormatText* FormatNode = NewObject<UK2Node_FormatText>(TargetGraph);
	TargetGraph->AddNode(FormatNode, true, false);
	FormatNode->CreateNewGuid();
	FormatNode->PostPlacedNewNode();
	FormatNode->NodePosX = static_cast<int32>(NodeData.X);
	FormatNode->NodePosY = static_cast<int32>(NodeData.Y);
	FormatNode->AllocateDefaultPins();

	// 设置格式字符串到 Format 引脚触发参数引脚创建
	const FString& FormatString = NodeData.FormatTextReference.FormatString;
	if (!FormatString.IsEmpty())
	{
		UEdGraphPin* FormatPin = FormatNode->FindPin(TEXT("Format"));
		if (!FormatPin)
		{
			FormatPin = FormatNode->FindPin(TEXT("FormatString"));
		}
		if (FormatPin)
		{
			FormatPin->DefaultTextValue = FText::FromString(FormatString);
			FormatNode->PinDefaultValueChanged(FormatPin);
		}
	}

	FormatNode->ReconstructNode();

	TextToBlueprintGenerator::ApplyDefaultValues(FormatNode, NodeData.DefaultValues);

	return FormatNode;
}
