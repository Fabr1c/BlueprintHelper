#include "Systems/ToolClusters/GraphWrite/NodeHandlers/AssignDelegateNodeHandler.h"

#include "K2Node_AssignDelegate.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"

bool FAssignDelegateNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::AssignDelegate;
}

UK2Node* FAssignDelegateNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("AssignDelegate 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& DelegateName = NodeData.DelegateReference.DelegatePropertyName;
	if (DelegateName.IsEmpty())
	{
		OutError = TEXT("AssignDelegate 节点生成失败：委托属性名为空，请。delegate.property_name 中指定。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint || !Blueprint->SkeletonGeneratedClass)
	{
		OutError = TEXT("AssignDelegate 节点生成失败：无法找到蓝图或骨架类。");
		return nullptr;
	}

	FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(
		Blueprint->SkeletonGeneratedClass, FName(*DelegateName));
	if (!DelegateProperty)
	{
		OutError = FString::Printf(TEXT("AssignDelegate 节点生成失败：未在蓝图中找到事件分发。'%s'。"), *DelegateName);
		return nullptr;
	}

	UK2Node_AssignDelegate* DelegateNode = NewObject<UK2Node_AssignDelegate>(TargetGraph);
	TargetGraph->AddNode(DelegateNode, true, false);
	DelegateNode->CreateNewGuid();
	DelegateNode->SetFromProperty(DelegateProperty, false, DelegateProperty->GetOwnerClass());
	DelegateNode->AllocateDefaultPins();
	// 不调。PostPlacedNewNode()：该方法会自动创建伴。CustomEvent 子节点并尝试连线。
	// 程序化创建时缺少完整上下文会导致空指针崩溃（EXCEPTION_ACCESS_VIOLATION）。
	DelegateNode->NodePosX = static_cast<int32>(NodeData.X);
	DelegateNode->NodePosY = static_cast<int32>(NodeData.Y);
	FBlueprintGraphWriteFacade::ApplyDefaultValues(DelegateNode, NodeData.DefaultValues);
	return DelegateNode;
}
