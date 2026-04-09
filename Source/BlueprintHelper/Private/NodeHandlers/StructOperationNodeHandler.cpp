#include "NodeHandlers/StructOperationNodeHandler.h"

#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "TextToBlueprintGenerator.h"
#include "UObject/UObjectIterator.h"

bool FStructOperationNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::MakeStruct
		|| NodeType == EParsedBlueprintNodeType::BreakStruct;
}

UK2Node* FStructOperationNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("StructOperation 节点生成失败：目标图表无效。");
		return nullptr;
	}

	switch (NodeData.NodeType)
	{
	case EParsedBlueprintNodeType::MakeStruct:
		return SpawnMakeStruct(TargetGraph, NodeData, OutError);
	case EParsedBlueprintNodeType::BreakStruct:
		return SpawnBreakStruct(TargetGraph, NodeData, OutError);
	default:
		OutError = TEXT("StructOperation 节点生成失败：意外的节点类型。");
		return nullptr;
	}
}

UScriptStruct* FStructOperationNodeHandler::ResolveScriptStruct(const FString& StructPath, FString& OutError)
{
	if (StructPath.IsEmpty())
	{
		OutError = TEXT("StructOperation 节点生成失败：struct_path 为空。");
		return nullptr;
	}

	UScriptStruct* FoundStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
	if (!FoundStruct)
	{
		FoundStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
	}

	if (!FoundStruct)
	{
		OutError = FString::Printf(TEXT("StructOperation 节点生成失败：未找到结构体 '%s'。"), *StructPath);
	}

	return FoundStruct;
}

UK2Node* FStructOperationNodeHandler::SpawnMakeStruct(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UScriptStruct* TargetStruct = ResolveScriptStruct(NodeData.StructReference.StructPath, OutError);
	if (!TargetStruct)
	{
		return nullptr;
	}

	UK2Node_MakeStruct* StructNode = NewObject<UK2Node_MakeStruct>(TargetGraph);
	StructNode->StructType = TargetStruct;
	TargetGraph->AddNode(StructNode, true, false);
	StructNode->CreateNewGuid();
	StructNode->AllocateDefaultPins();
	StructNode->PostPlacedNewNode();

	StructNode->NodePosX = static_cast<int32>(NodeData.X);
	StructNode->NodePosY = static_cast<int32>(NodeData.Y);

	TextToBlueprintGenerator::ApplyDefaultValues(StructNode, NodeData.DefaultValues);
	return StructNode;
}

UK2Node* FStructOperationNodeHandler::SpawnBreakStruct(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	UScriptStruct* TargetStruct = ResolveScriptStruct(NodeData.StructReference.StructPath, OutError);
	if (!TargetStruct)
	{
		return nullptr;
	}

	UK2Node_BreakStruct* StructNode = NewObject<UK2Node_BreakStruct>(TargetGraph);
	StructNode->StructType = TargetStruct;
	TargetGraph->AddNode(StructNode, true, false);
	StructNode->CreateNewGuid();
	StructNode->AllocateDefaultPins();
	StructNode->PostPlacedNewNode();

	StructNode->NodePosX = static_cast<int32>(NodeData.X);
	StructNode->NodePosY = static_cast<int32>(NodeData.Y);

	TextToBlueprintGenerator::ApplyDefaultValues(StructNode, NodeData.DefaultValues);
	return StructNode;
}
