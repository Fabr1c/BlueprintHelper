#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SpawnActorNodeHandler.h"

#include "K2Node_SpawnActorFromClass.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "UObject/UObjectGlobals.h"

bool FSpawnActorNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::SpawnActorFromClass;
}

UK2Node* FSpawnActorNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("SpawnActor 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node_SpawnActorFromClass* SpawnNode = NewObject<UK2Node_SpawnActorFromClass>(TargetGraph);
	TargetGraph->AddNode(SpawnNode, true, false);
	SpawnNode->CreateNewGuid();
	SpawnNode->NodePosX = static_cast<int32>(NodeData.X);
	SpawnNode->NodePosY = static_cast<int32>(NodeData.Y);
	SpawnNode->AllocateDefaultPins();
	SpawnNode->PostPlacedNewNode();

	// 如果指定。Actor 类路径，设置 Class 引脚默认。
	const FString& ClassPath = NodeData.SpawnReference.ClassPath;
	if (!ClassPath.IsEmpty())
	{
		UClass* ActorClass = FindObject<UClass>(nullptr, *ClassPath);
		if (!ActorClass)
		{
			ActorClass = LoadObject<UClass>(nullptr, *ClassPath);
		}

		if (ActorClass)
		{
			UEdGraphPin* ClassPin = SpawnNode->GetClassPin();
			if (ClassPin)
			{
				ClassPin->DefaultObject = ActorClass;
				SpawnNode->PinDefaultValueChanged(ClassPin);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnActor: 未找到类 '%s'，Class 引脚留空。"), *ClassPath);
		}
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(SpawnNode, NodeData.DefaultValues);

	return SpawnNode;
}
