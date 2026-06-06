#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UK2Node_Tunnel;
struct FBlueprintHelperGraphBodyBoundaryModel;

class FBlueprintHelperK2GraphBodyAdapterUtils
{
public:
	static UEdGraph* FindGraphByName(const TArray<UEdGraph*>& Graphs, const FString& GraphName);
	static FString NodeRef(const UEdGraphNode* Node);
	static bool IsFunctionEntry(const UEdGraphNode* Node);
	static bool IsFunctionResult(const UEdGraphNode* Node);
	static bool IsTunnelEntry(const UK2Node_Tunnel* Tunnel);
	static bool IsTunnelExit(const UK2Node_Tunnel* Tunnel);
	static bool IsCustomEventEntry(const UEdGraphNode* Node, const FString& EntryName);
	static bool IsEventEntry(const UEdGraphNode* Node, const FString& EntryName);
	static bool HasExecPin(const UEdGraphNode* Node, EEdGraphPinDirection Direction);
	static bool TryReadBlueprintHelperBlockId(const UEdGraphNode* Node, FString& OutBlockId);
	static void AppendOwnedBodyNodesForBlock(
		UEdGraph* Graph,
		const FString& BlockId,
		const UEdGraphNode* EntryNode,
		TArray<UEdGraphNode*>& OutNodes);
	static void AppendPinSemanticSources(
		const UEdGraphNode* Node,
		const FString& NodeRef,
		TArray<FString>& OutSemanticSourceRefs);
};
