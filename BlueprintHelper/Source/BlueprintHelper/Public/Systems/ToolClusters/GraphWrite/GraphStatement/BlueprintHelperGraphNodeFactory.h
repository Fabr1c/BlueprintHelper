#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class BLUEPRINTHELPER_API FBlueprintHelperGraphNodeFactory
{
public:
	template <typename TNode>
	static TNode* SpawnK2Node(UEdGraph* TargetGraph, const FVector2D& Location)
	{
		return SpawnK2Node<TNode>(
			TargetGraph,
			Location,
			[](TNode*) {});
	}

	template <typename TNode, typename TConfigureBeforeAllocate>
	static TNode* SpawnK2Node(
		UEdGraph* TargetGraph,
		const FVector2D& Location,
		TConfigureBeforeAllocate ConfigureBeforeAllocate)
	{
		if (!TargetGraph)
		{
			return nullptr;
		}

		TNode* Node = NewObject<TNode>(TargetGraph);
		TargetGraph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		ConfigureBeforeAllocate(Node);
		Node->NodePosX = static_cast<int32>(Location.X);
		Node->NodePosY = static_cast<int32>(Location.Y);
		Node->AllocateDefaultPins();
		return Node;
	}
};
