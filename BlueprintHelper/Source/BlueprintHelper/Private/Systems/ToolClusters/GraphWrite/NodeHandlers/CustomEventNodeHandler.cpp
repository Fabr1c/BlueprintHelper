#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CustomEventNodeHandler.h"

#include "K2Node_CustomEvent.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"
#include "EdGraphSchema_K2.h"

bool FCustomEventNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CustomEvent;
}

UK2Node* FCustomEventNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("CustomEvent 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& EventName = NodeData.EventReference.EventName;
	if (EventName.IsEmpty())
	{
		OutError = TEXT("CustomEvent 节点生成失败：事件名为空，请。event.event_name 中指定。");
		return nullptr;
	}

	TArray<TPair<FString, FEdGraphPinType>> ConvertedParams;
	for (const FParsedEventParam& Param : NodeData.EventReference.Params)
	{
		FEdGraphPinType PinType;
		FString ConvertError;
		if (!FBlueprintGraphWriteFacade::ConvertToEdGraphPinType(Param.PinType, PinType, ConvertError))
		{
			OutError = FString::Printf(TEXT("CustomEvent 参数 '%s' 类型转换失败：%s"), *Param.Name, *ConvertError);
			return nullptr;
		}
		ConvertedParams.Add(TPair<FString, FEdGraphPinType>(Param.Name, PinType));
	}

	UK2Node_CustomEvent* EventNode =
		FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_CustomEvent>(
			TargetGraph,
			FVector2D(NodeData.X, NodeData.Y),
			[&EventName](UK2Node_CustomEvent* Node)
			{
				Node->CustomFunctionName = FName(*EventName);
			});

	for (const TPair<FString, FEdGraphPinType>& Param : ConvertedParams)
	{
		EventNode->CreateUserDefinedPin(*Param.Key, Param.Value, EGPD_Output);
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(EventNode, NodeData.DefaultValues);
	return EventNode;
}
