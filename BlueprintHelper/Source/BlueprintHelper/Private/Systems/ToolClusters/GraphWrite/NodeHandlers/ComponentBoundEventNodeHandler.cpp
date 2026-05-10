#include "Systems/ToolClusters/GraphWrite/NodeHandlers/ComponentBoundEventNodeHandler.h"
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

bool FComponentBoundEventNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::ComponentBoundEvent;
}

UK2Node* FComponentBoundEventNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("ComponentBoundEvent 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FParsedComponentBoundEventReference& Ref = NodeData.ComponentBoundEventReference;

	if (Ref.DelegatePropertyName.IsEmpty() || Ref.ComponentPropertyName.IsEmpty())
	{
		OutError = TEXT("ComponentBoundEvent 节点生成失败：缺。delegate_property 。component_property。");
		return nullptr;
	}

	// 解析委托所属类
	UClass* DelegateOwnerClass = nullptr;
	if (!Ref.DelegateOwnerClassPath.IsEmpty())
	{
		DelegateOwnerClass = FindObject<UClass>(nullptr, *Ref.DelegateOwnerClassPath);
		if (!DelegateOwnerClass)
		{
			DelegateOwnerClass = LoadObject<UClass>(nullptr, *Ref.DelegateOwnerClassPath);
		}
	}

	// 查找蓝图上的组件属性和委托属性进行初始化
	UBlueprint* Blueprint = TargetGraph->GetTypedOuter<UBlueprint>();
	UClass* BlueprintClass = Blueprint ? Blueprint->SkeletonGeneratedClass : nullptr;

	FObjectProperty* ComponentProperty = nullptr;
	FMulticastDelegateProperty* DelegateProperty = nullptr;

	if (BlueprintClass)
	{
		ComponentProperty = FindFProperty<FObjectProperty>(BlueprintClass, *Ref.ComponentPropertyName);
		if (ComponentProperty && ComponentProperty->PropertyClass)
		{
			UClass* SearchClass = DelegateOwnerClass ? DelegateOwnerClass : ComponentProperty->PropertyClass.Get();
			DelegateProperty = FindFProperty<FMulticastDelegateProperty>(SearchClass, *Ref.DelegatePropertyName);
		}
	}

	UK2Node_ComponentBoundEvent* EventNode = NewObject<UK2Node_ComponentBoundEvent>(TargetGraph);
	TargetGraph->AddNode(EventNode, true, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->NodePosX = static_cast<int32>(NodeData.X);
	EventNode->NodePosY = static_cast<int32>(NodeData.Y);

	if (ComponentProperty && DelegateProperty)
	{
		EventNode->InitializeComponentBoundEventParams(ComponentProperty, DelegateProperty);
	}
	else
	{
		// 备用路径：直接设置属性，让节点在 ReconstructNode 时自行解析
		EventNode->DelegatePropertyName = FName(*Ref.DelegatePropertyName);
		EventNode->ComponentPropertyName = FName(*Ref.ComponentPropertyName);
		if (DelegateOwnerClass)
		{
			EventNode->DelegateOwnerClass = DelegateOwnerClass;
		}
	}

	EventNode->AllocateDefaultPins();
	return EventNode;
}
