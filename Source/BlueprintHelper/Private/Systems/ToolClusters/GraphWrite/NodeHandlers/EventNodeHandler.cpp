#include "Systems/ToolClusters/GraphWrite/NodeHandlers/EventNodeHandler.h"

#include "K2Node_Event.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"

class FEventNodeHandlerLocalUtils
{
public:
	/**
	 * 常见引擎事件名称映射，将用户友好名称转换为实际函数名。
	 */
	static FName ResolveEngineEventFunctionName(const FString& InEventName)
	{
		const FString Lower = InEventName.ToLower();
		if (Lower == TEXT("beginplay") || Lower == TEXT("receivebeginplay"))
		{
			return FName(TEXT("ReceiveBeginPlay"));
		}
		if (Lower == TEXT("tick") || Lower == TEXT("receivetick"))
		{
			return FName(TEXT("ReceiveTick"));
		}
		if (Lower == TEXT("endplay") || Lower == TEXT("receiveendplay"))
		{
			return FName(TEXT("ReceiveEndPlay"));
		}
		if (Lower == TEXT("anydamage") || Lower == TEXT("receiveanydamage"))
		{
			return FName(TEXT("ReceiveAnyDamage"));
		}
		if (Lower == TEXT("actorendoverlap") || Lower == TEXT("receiveactorendoverlap"))
		{
			return FName(TEXT("ReceiveActorEndOverlap"));
		}
		if (Lower == TEXT("actorbeginoverlap") || Lower == TEXT("receiveactorbeginoverlap"))
		{
			return FName(TEXT("ReceiveActorBeginOverlap"));
		}
		if (Lower == TEXT("destroyed") || Lower == TEXT("receivedestroyed"))
		{
			return FName(TEXT("ReceiveDestroyed"));
		}
		if (Lower == TEXT("actorhit") || Lower == TEXT("receiveactorhit") || Lower == TEXT("hit"))
		{
			return FName(TEXT("ReceiveHit"));
		}
		// 如果没有匹配到常见名称，尝试自动添加 Receive 前缀
		if (!InEventName.StartsWith(TEXT("Receive"), ESearchCase::IgnoreCase))
		{
			return FName(*(TEXT("Receive") + InEventName));
		}
		return FName(*InEventName);
	}

};

bool FEventNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Event;
}

UK2Node* FEventNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Event 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& EventName = NodeData.EventReference.EventName;
	if (EventName.IsEmpty())
	{
		OutError = TEXT("Event 节点生成失败：事件名为空，请。event.event_name 中指定。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint)
	{
		OutError = TEXT("Event 节点生成失败：无法找到目标图表所属蓝图。");
		return nullptr;
	}

	UClass* ParentClass = Blueprint->ParentClass;
	if (!ParentClass)
	{
		OutError = TEXT("Event 节点生成失败：蓝图父类无效。");
		return nullptr;
	}

	const FName ResolvedEventName = FEventNodeHandlerLocalUtils::ResolveEngineEventFunctionName(EventName);
	UFunction* EventFunction = ParentClass->FindFunctionByName(ResolvedEventName);
	if (!EventFunction)
	{
		OutError = FString::Printf(TEXT("Event 节点生成失败：在蓝图父类 '%s' 中未找到事件函数 '%s'。"),
			*ParentClass->GetName(), *ResolvedEventName.ToString());
		return nullptr;
	}

	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(TargetGraph);
	TargetGraph->AddNode(EventNode, true, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->EventReference.SetExternalMember(ResolvedEventName, ParentClass);
	EventNode->bOverrideFunction = true;
	EventNode->NodePosX = static_cast<int32>(NodeData.X);
	EventNode->NodePosY = static_cast<int32>(NodeData.Y);
	EventNode->AllocateDefaultPins();
	TextToBlueprintGenerator::ApplyDefaultValues(EventNode, NodeData.DefaultValues);
	return EventNode;
}
