#include "Systems/ToolClusters/GraphWrite/NodeHandlers/EnhancedInputActionNodeHandler.h"

#include "K2Node_EnhancedInputAction.h"
#include "InputAction.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

bool FEnhancedInputActionNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::EnhancedInputAction;
}

UK2Node* FEnhancedInputActionNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("EnhancedInputAction 节点生成失败：目标图表无效。");
		return nullptr;
	}

	const FString& InputActionPath = NodeData.EnhancedInputActionReference.InputActionPath;
	if (InputActionPath.IsEmpty())
	{
		OutError = TEXT("EnhancedInputAction 节点生成失败：input_action_path 为空。");
		return nullptr;
	}

	// 加载 InputAction 资产
	UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!InputAction)
	{
		// 尝试添加类路径前缀
		const FString AlternativePath = FString::Printf(TEXT("/Script/EnhancedInput.InputAction'%s'"), *InputActionPath);
		InputAction = LoadObject<UInputAction>(nullptr, *AlternativePath);
	}
	if (!InputAction)
	{
		OutError = FString::Printf(TEXT("EnhancedInputAction 节点生成失败：未找到 InputAction 资产 '%s'。"), *InputActionPath);
		return nullptr;
	}

	UK2Node_EnhancedInputAction* NewNode = NewObject<UK2Node_EnhancedInputAction>(TargetGraph);
	TargetGraph->AddNode(NewNode, true, false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->InputAction = InputAction;
	NewNode->NodePosX = static_cast<int32>(NodeData.X);
	NewNode->NodePosY = static_cast<int32>(NodeData.Y);
	NewNode->AllocateDefaultPins();

	FBlueprintGraphWriteFacade::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	return NewNode;
}
