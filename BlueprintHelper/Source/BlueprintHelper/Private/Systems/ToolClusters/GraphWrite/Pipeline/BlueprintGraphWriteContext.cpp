#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"

void FBlueprintGraphWriteContext::Initialize(UEdGraph* InGraph)
{
	Graph = InGraph;
	IdToNode.Empty();
	PinIndexByNodeKey.Empty();
	GeneratedNodes.Empty();
	EntryRootNodes.Empty();
}

bool FBlueprintGraphWriteContext::IsValid() const
{
	return Graph != nullptr;
}

UEdGraph* FBlueprintGraphWriteContext::GetGraph() const
{
	return Graph;
}

void FBlueprintGraphWriteContext::RegisterNode(
	const FString& NodeId,
	UK2Node* Node,
	bool bGenerated,
	bool bEntryRoot)
{
	if (NodeId.IsEmpty() || !Node)
	{
		return;
	}

	IdToNode.FindOrAdd(NodeId, Node);
	if (bGenerated)
	{
		GeneratedNodes.AddUnique(Node);
	}
	if (bEntryRoot)
	{
		EntryRootNodes.Add(Node);
	}
	BuildPinIndex(Node);
}

UK2Node* FBlueprintGraphWriteContext::FindNode(const FString& NodeId) const
{
	if (UK2Node* const* Found = IdToNode.Find(NodeId))
	{
		return *Found;
	}
	return nullptr;
}

UEdGraphPin* FBlueprintGraphWriteContext::FindPinByAlias(
	const FString& NodeId,
	const FString& RequestedPinName)
{
	UK2Node* Node = FindNode(NodeId);
	if (!Node)
	{
		return nullptr;
	}

	const FString NodeKey = MakePinLookupKey(Node);
	if (!PinIndexByNodeKey.Contains(NodeKey))
	{
		BuildPinIndex(Node);
	}

	const FString NormalizedPin = FBlueprintGraphNodeUtility::NormalizePinKey(RequestedPinName);
	if (TMap<FString, UEdGraphPin*>* PinIndex = PinIndexByNodeKey.Find(NodeKey))
	{
		if (UEdGraphPin** FoundPin = PinIndex->Find(NormalizedPin))
		{
			return *FoundPin;
		}
	}
	return FBlueprintGraphNodeUtility::FindPinByAlias(Node, RequestedPinName);
}

const TArray<UEdGraphNode*>& FBlueprintGraphWriteContext::GetGeneratedNodes() const
{
	return GeneratedNodes;
}

const TSet<UEdGraphNode*>& FBlueprintGraphWriteContext::GetEntryRootNodes() const
{
	return EntryRootNodes;
}

void FBlueprintGraphWriteContext::BuildPinIndex(UK2Node* Node)
{
	if (!Node)
	{
		return;
	}

	TMap<FString, UEdGraphPin*> PinIndex;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		PinIndex.FindOrAdd(FBlueprintGraphNodeUtility::NormalizePinKey(Pin->PinName.ToString()), Pin);
		PinIndex.FindOrAdd(FBlueprintGraphNodeUtility::NormalizePinKey(Pin->GetDisplayName().ToString()), Pin);
	}
	PinIndexByNodeKey.Add(MakePinLookupKey(Node), MoveTemp(PinIndex));
}

FString FBlueprintGraphWriteContext::MakePinLookupKey(UK2Node* Node)
{
	return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : TEXT("");
}
