// BlueprintHelper Service Layer - external body snapshot service.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Misc/Crc.h"

namespace BlueprintHelperExternalBodySnapshot
{
	static FString NodeGuidString(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
	}

	static FString NodeClassPath(const UEdGraphNode* Node)
	{
		return Node && Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
	}

	static FString PinDirectionToString(const EEdGraphPinDirection Direction)
	{
		switch (Direction)
		{
		case EGPD_Input:
			return TEXT("input");
		case EGPD_Output:
			return TEXT("output");
		default:
			return TEXT("unknown");
		}
	}

	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	struct FQueuedBodyNode
	{
		UEdGraphNode* Node = nullptr;
		bool bFollowExecOutputs = false;
	};

	static FBlueprintHelperExternalBodyEndpoint MakeEndpoint(const UEdGraphPin* Pin)
	{
		FBlueprintHelperExternalBodyEndpoint Endpoint;
		const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
		Endpoint.NodeGuid = NodeGuidString(Node);
		Endpoint.PinName = Pin ? Pin->PinName.ToString() : FString();
		Endpoint.PinDirection = Pin ? PinDirectionToString(Pin->Direction) : FString();
		return Endpoint;
	}

	static void AddStringArray(
		const TSharedRef<FJsonObject>& Json,
		const TCHAR* FieldName,
		const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FString& Value : Values)
		{
			Array.Add(MakeShared<FJsonValueString>(Value));
		}
		Json->SetArrayField(FieldName, Array);
	}

	static TArray<TSharedPtr<FJsonValue>> LinkArrayToJson(
		const TArray<FBlueprintHelperExternalBodyLink>& Links)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FBlueprintHelperExternalBodyLink& Link : Links)
		{
			Array.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
		}
		return Array;
	}

	static FString PinStableText(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}
		return FString::Printf(
			TEXT("pin:%s|%s|%s|%s|%s|default=%s"),
			*Pin->PinName.ToString(),
			*PinDirectionToString(Pin->Direction),
			*Pin->PinType.PinCategory.ToString(),
			*Pin->PinType.PinSubCategory.ToString(),
			Pin->PinType.PinSubCategoryObject.IsValid() ? *Pin->PinType.PinSubCategoryObject->GetPathName() : TEXT(""),
			*Pin->DefaultValue);
	}
}

FString FBlueprintHelperExternalBodyEndpoint::ToStableString() const
{
	return FString::Printf(TEXT("%s.%s.%s"), *NodeGuid, *PinName, *PinDirection);
}

TSharedRef<FJsonObject> FBlueprintHelperExternalBodyEndpoint::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("node_guid"), NodeGuid);
	Json->SetStringField(TEXT("pin_name"), PinName);
	Json->SetStringField(TEXT("pin_direction"), PinDirection);
	return Json;
}

FString FBlueprintHelperExternalBodyLink::ToStableString() const
{
	return FString::Printf(TEXT("%s->%s"), *From.ToStableString(), *To.ToStableString());
}

TSharedRef<FJsonObject> FBlueprintHelperExternalBodyLink::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("from"), From.ToJson());
	Json->SetObjectField(TEXT("to"), To.ToJson());
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperExternalBodySnapshot::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ExternalBodySnapshot.v1"));
	Json->SetStringField(TEXT("entry_node_guid"), EntryNodeGuid);
	Json->SetStringField(TEXT("entry_node_class"), EntryNodeClass);
	Json->SetStringField(TEXT("body_fingerprint"), BodyFingerprint);
	Json->SetStringField(TEXT("restore_text"), RestoreText);
	BlueprintHelperExternalBodySnapshot::AddStringArray(Json, TEXT("body_node_guids"), BodyNodeGuids);
	Json->SetArrayField(TEXT("entry_to_body_links"), BlueprintHelperExternalBodySnapshot::LinkArrayToJson(EntryToBodyLinks));
	Json->SetArrayField(TEXT("body_to_external_links"), BlueprintHelperExternalBodySnapshot::LinkArrayToJson(BodyToExternalLinks));
	return Json;
}

TArray<UEdGraphNode*> FBlueprintHelperExternalBodySnapshotService::CollectBodyNodes(
	UEdGraph* Graph,
	UEdGraphNode* EntryNode) const
{
	TArray<UEdGraphNode*> BodyNodes;
	if (!Graph || !EntryNode)
	{
		return BodyNodes;
	}

	TSet<UEdGraphNode*> Visited;
	TSet<UEdGraphNode*> ExecScanned;
	TSet<UEdGraphNode*> DataScanned;
	TArray<BlueprintHelperExternalBodySnapshot::FQueuedBodyNode> Queue;
	auto EnqueueBodyNode = [&BodyNodes, &EntryNode, &Queue, &Visited](
		UEdGraphNode* Node,
		const bool bFollowExecOutputs)
	{
		if (!Node || Node == EntryNode)
		{
			return;
		}
		if (!Visited.Contains(Node))
		{
			Visited.Add(Node);
			BodyNodes.Add(Node);
		}
		Queue.Add({ Node, bFollowExecOutputs });
	};

	for (UEdGraphPin* Pin : EntryNode->Pins)
	{
		if (!BlueprintHelperExternalBodySnapshot::IsExecPin(Pin) || Pin->Direction != EGPD_Output)
		{
			continue;
		}
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			EnqueueBodyNode(LinkedNode, true);
		}
	}

	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		UEdGraphNode* Node = Queue[Index].Node;
		if (!Node)
		{
			continue;
		}

		if (Queue[Index].bFollowExecOutputs && !ExecScanned.Contains(Node))
		{
			ExecScanned.Add(Node);
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!BlueprintHelperExternalBodySnapshot::IsExecPin(Pin) || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					EnqueueBodyNode(LinkedPin ? LinkedPin->GetOwningNode() : nullptr, true);
				}
			}
		}

		if (DataScanned.Contains(Node))
		{
			continue;
		}
		DataScanned.Add(Node);
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin
				|| Pin->Direction != EGPD_Input
				|| BlueprintHelperExternalBodySnapshot::IsExecPin(Pin))
			{
				continue;
			}
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin
					|| LinkedPin->Direction != EGPD_Output
					|| BlueprintHelperExternalBodySnapshot::IsExecPin(LinkedPin))
				{
					continue;
				}
				EnqueueBodyNode(LinkedPin->GetOwningNode(), false);
			}
		}
	}

	BodyNodes.Sort([](const UEdGraphNode& Left, const UEdGraphNode& Right)
	{
		return BlueprintHelperExternalBodySnapshot::NodeGuidString(&Left) < BlueprintHelperExternalBodySnapshot::NodeGuidString(&Right);
	});
	return BodyNodes;
}

bool FBlueprintHelperExternalBodySnapshotService::CaptureBody(
	UEdGraph* Graph,
	UEdGraphNode* EntryNode,
	FBlueprintHelperExternalBodySnapshot& OutSnapshot,
	FString& OutError) const
{
	OutSnapshot = FBlueprintHelperExternalBodySnapshot();
	OutError.Reset();

	if (!Graph || !EntryNode)
	{
		OutError = TEXT("external_body_entry_not_found");
		return false;
	}

	OutSnapshot.EntryNodeGuid = BlueprintHelperExternalBodySnapshot::NodeGuidString(EntryNode);
	OutSnapshot.EntryNodeClass = BlueprintHelperExternalBodySnapshot::NodeClassPath(EntryNode);

	const TArray<UEdGraphNode*> BodyNodes = CollectBodyNodes(Graph, EntryNode);
	TSet<UEdGraphNode*> BodySet;
	TSet<UObject*> ExportNodes;
	for (UEdGraphNode* Node : BodyNodes)
	{
		if (!Node)
		{
			continue;
		}
		BodySet.Add(Node);
		ExportNodes.Add(Node);
		OutSnapshot.BodyNodeGuids.Add(BlueprintHelperExternalBodySnapshot::NodeGuidString(Node));
	}

	for (UEdGraphPin* EntryPin : EntryNode->Pins)
	{
		if (!EntryPin)
		{
			continue;
		}
		for (UEdGraphPin* LinkedPin : EntryPin->LinkedTo)
		{
			UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (LinkedNode && BodySet.Contains(LinkedNode))
			{
				FBlueprintHelperExternalBodyLink Link;
				Link.From = BlueprintHelperExternalBodySnapshot::MakeEndpoint(EntryPin);
				Link.To = BlueprintHelperExternalBodySnapshot::MakeEndpoint(LinkedPin);
				OutSnapshot.EntryToBodyLinks.Add(Link);
			}
		}
	}

	for (UEdGraphNode* Node : BodyNodes)
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
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedNode || BodySet.Contains(LinkedNode) || LinkedNode == EntryNode)
				{
					continue;
				}
				FBlueprintHelperExternalBodyLink Link;
				Link.From = BlueprintHelperExternalBodySnapshot::MakeEndpoint(Pin);
				Link.To = BlueprintHelperExternalBodySnapshot::MakeEndpoint(LinkedPin);
				OutSnapshot.BodyToExternalLinks.Add(Link);
			}
		}
	}

	OutSnapshot.EntryToBodyLinks.Sort([](const FBlueprintHelperExternalBodyLink& A, const FBlueprintHelperExternalBodyLink& B)
	{
		return A.ToStableString() < B.ToStableString();
	});
	OutSnapshot.BodyToExternalLinks.Sort([](const FBlueprintHelperExternalBodyLink& A, const FBlueprintHelperExternalBodyLink& B)
	{
		return A.ToStableString() < B.ToStableString();
	});

	if (ExportNodes.Num() > 0)
	{
		FEdGraphUtilities::ExportNodesToText(ExportNodes, OutSnapshot.RestoreText);
	}
	OutSnapshot.BodyFingerprint = ComputeBodyFingerprint(
		EntryNode,
		BodyNodes,
		OutSnapshot.EntryToBodyLinks,
		OutSnapshot.BodyToExternalLinks);
	return true;
}

FString FBlueprintHelperExternalBodySnapshotService::ComputeBodyFingerprint(
	UEdGraphNode* EntryNode,
	const TArray<UEdGraphNode*>& BodyNodes,
	const TArray<FBlueprintHelperExternalBodyLink>& EntryToBodyLinks,
	const TArray<FBlueprintHelperExternalBodyLink>& BodyToExternalLinks)
{
	TArray<FString> NodeTexts;
	for (UEdGraphNode* Node : BodyNodes)
	{
		if (!Node)
		{
			continue;
		}
		TArray<FString> PinTexts;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			PinTexts.Add(BlueprintHelperExternalBodySnapshot::PinStableText(Pin));
		}
		PinTexts.Sort();
		NodeTexts.Add(FString::Printf(
			TEXT("node:%s|class=%s|comment=%s|pins=%s"),
			*BlueprintHelperExternalBodySnapshot::NodeGuidString(Node),
			*BlueprintHelperExternalBodySnapshot::NodeClassPath(Node),
			*Node->NodeComment,
			*FString::Join(PinTexts, TEXT(","))));
	}
	NodeTexts.Sort();

	TArray<FString> EntryLinks;
	for (const FBlueprintHelperExternalBodyLink& Link : EntryToBodyLinks)
	{
		EntryLinks.Add(Link.ToStableString());
	}
	EntryLinks.Sort();

	TArray<FString> ExternalLinks;
	for (const FBlueprintHelperExternalBodyLink& Link : BodyToExternalLinks)
	{
		ExternalLinks.Add(Link.ToStableString());
	}
	ExternalLinks.Sort();

	const FString StableText = FString::Printf(
		TEXT("external-body|entry=%s|entry_class=%s|nodes=%s|entry_links=%s|external_links=%s"),
		*BlueprintHelperExternalBodySnapshot::NodeGuidString(EntryNode),
		*BlueprintHelperExternalBodySnapshot::NodeClassPath(EntryNode),
		*FString::Join(NodeTexts, TEXT("|")),
		*FString::Join(EntryLinks, TEXT("|")),
		*FString::Join(ExternalLinks, TEXT("|")));
	return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*StableText));
}
