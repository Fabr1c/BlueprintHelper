#include "NodeHandlers/GraphWrite/CallDelegateNodeHandler.h"

#include "K2Node_CallDelegate.h"
#include "GraphWrite/TextToBlueprintGenerator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"

bool FCallDelegateNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CallDelegate;
}

UK2Node* FCallDelegateNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("CallDelegate 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& DelegateName = NodeData.DelegateReference.DelegatePropertyName;
	if (DelegateName.IsEmpty())
	{
		OutError = TEXT("CallDelegate 节点生成失败：委托属性名为空，请。delegate.property_name 中指定。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint || !Blueprint->SkeletonGeneratedClass)
	{
		OutError = TEXT("CallDelegate 节点生成失败：无法找到蓝图或骨架类。");
		return nullptr;
	}

	FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(
		Blueprint->SkeletonGeneratedClass, FName(*DelegateName));
	if (!DelegateProperty)
	{
		OutError = FString::Printf(TEXT("CallDelegate 节点生成失败：未在蓝图中找到事件分发。'%s'。"), *DelegateName);
		return nullptr;
	}

	UK2Node_CallDelegate* DelegateNode = NewObject<UK2Node_CallDelegate>(TargetGraph);
	TargetGraph->AddNode(DelegateNode, true, false);
	DelegateNode->CreateNewGuid();
	DelegateNode->SetFromProperty(DelegateProperty, false, DelegateProperty->GetOwnerClass());
	DelegateNode->PostPlacedNewNode();
	DelegateNode->NodePosX = static_cast<int32>(NodeData.X);
	DelegateNode->NodePosY = static_cast<int32>(NodeData.Y);
	DelegateNode->AllocateDefaultPins();
	TextToBlueprintGenerator::ApplyDefaultValues(DelegateNode, NodeData.DefaultValues);
	return DelegateNode;
}
