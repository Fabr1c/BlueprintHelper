#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSnapshotBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

namespace BlueprintHelper::GraphLayout
{
static FString MakeNodeId(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	return Node->NodeGuid.IsValid() ? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : Node->GetName();
}

static FString MakePinId(const UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return FString();
	}
	return Pin->PinId.IsValid() ? Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens) : Pin->PinName.ToString();
}

static FString GetNodeTitle(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	return Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
}

FNodeSnapshot FSnapshotBuilder::CaptureNode(const UEdGraphNode* Node)
{
	FNodeSnapshot Snapshot;
	if (!Node)
	{
		return Snapshot;
	}

	Snapshot.NodeId = MakeNodeId(Node);
	Snapshot.StableName = Node->GetName();
	Snapshot.ClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
	Snapshot.Title = GetNodeTitle(Node);
	Snapshot.Position = FVector2D(static_cast<float>(Node->NodePosX), static_cast<float>(Node->NodePosY));
	Snapshot.Size = FVector2D(
		Node->NodeWidth > 0 ? static_cast<float>(Node->NodeWidth) : Snapshot.Size.X,
		Node->NodeHeight > 0 ? static_cast<float>(Node->NodeHeight) : Snapshot.Size.Y);
	Snapshot.bExisting = true;

	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		FPinSnapshot PinSnapshot;
		PinSnapshot.PinId = MakePinId(Pin);
		PinSnapshot.Name = Pin->PinName.ToString();
		PinSnapshot.Direction = Pin->Direction == EGPD_Output ? EPinDirection::Output : EPinDirection::Input;
		PinSnapshot.Category = Pin->PinType.PinCategory.ToString();
		PinSnapshot.bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				PinSnapshot.LinkedNodeIds.AddUnique(MakeNodeId(LinkedPin->GetOwningNode()));
			}
		}

		Snapshot.Pins.Add(PinSnapshot);
	}

	return Snapshot;
}

FGraphSnapshot FSnapshotBuilder::CaptureGraph(const UEdGraph* Graph)
{
	FGraphSnapshot Snapshot;
	if (!Graph)
	{
		return Snapshot;
	}

	Snapshot.GraphName = Graph->GetName();
	Snapshot.Nodes.Reserve(Graph->Nodes.Num());
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Snapshot.Nodes.Add(CaptureNode(Node));
		}
	}
	return Snapshot;
}
}
