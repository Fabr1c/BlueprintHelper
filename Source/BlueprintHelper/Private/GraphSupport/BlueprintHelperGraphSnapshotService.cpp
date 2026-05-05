// BlueprintHelper Service Layer — 图表快照服务实现

#include "GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

FBlueprintHelperGraphSnapshot FBlueprintHelperGraphSnapshotService::CaptureNodeSnapshot(
	UEdGraph* Graph,
	const TArray<UEdGraphNode*>& Nodes) const
{
	FBlueprintHelperGraphSnapshot Snapshot;
	if (!Graph)
	{
		return Snapshot;
	}

	Snapshot.GraphName = Graph->GetName();

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		Snapshot.NodeGuids.Add(Node->NodeGuid.ToString());
		Snapshot.NodeClasses.Add(Node->GetClass()->GetName());
		Snapshot.NodeTitles.Add(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		Snapshot.NodePositions.Add(FVector2D(Node->NodePosX, Node->NodePosY));
		Snapshot.NodeComments.Add(Node->NodeComment);

		// Pin 摘要
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				FString PinSummary = FString::Printf(TEXT("%s:%s[%s]"),
					*Pin->PinName.ToString(),
					Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
					*Pin->PinType.PinCategory.ToString());
				if (!Pin->DefaultValue.IsEmpty())
				{
					PinSummary += FString::Printf(TEXT("=%s"), *Pin->DefaultValue);
				}
				Snapshot.PinSummaries.Add(PinSummary);
			}
		}

		// Ownership metadata
		UPackage* Package = Node->GetOutermost();
		if (Package)
		{
			FMetaData& MetaData = Package->GetMetaData();
			const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
			if (!OwnedStr.IsEmpty())
			{
				const FString BlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
				Snapshot.OwnershipMetadata.Add(FString::Printf(TEXT("%s:owned=%s,block=%s"),
					*Node->GetName(), *OwnedStr, *BlockId));
			}
		}
	}

	// Link 摘要
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			for (UEdGraphPin* LinkedTo : Pin->LinkedTo)
			{
				if (LinkedTo && Nodes.Contains(LinkedTo->GetOwningNode()))
				{
					Snapshot.LinkSummaries.Add(FString::Printf(TEXT("%s.%s->%s.%s"),
						*Node->GetName(), *Pin->PinName.ToString(),
						*LinkedTo->GetOwningNode()->GetName(), *LinkedTo->PinName.ToString()));
				}
			}
		}
	}

	return Snapshot;
}

FBlueprintHelperGraphSnapshot FBlueprintHelperGraphSnapshotService::CaptureGraphSnapshot(
	UEdGraph* Graph) const
{
	if (!Graph)
	{
		return FBlueprintHelperGraphSnapshot();
	}

	TArray<UEdGraphNode*> AllNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			AllNodes.Add(Node);
		}
	}

	return CaptureNodeSnapshot(Graph, AllNodes);
}
