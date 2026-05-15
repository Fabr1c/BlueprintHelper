// BlueprintHelper Review target hash helpers implementation.

#include "Systems/Review/BlueprintHelperReviewHashService.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Misc/Crc.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

FString FBlueprintHelperReviewHashService::MakeStableHash(const FString& Payload)
{
	return FString::Printf(TEXT("crc32_%08x"), FCrc::StrCrc32(*Payload));
}

UBlueprint* FBlueprintHelperReviewHashService::LoadBlueprint(const FString& AssetPath, FString& OutError)
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path_empty");
		return nullptr;
	}

	UObject* Loaded = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(Loaded);
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *AssetPath);
		return nullptr;
	}
	return Blueprint;
}

UEdGraph* FBlueprintHelperReviewHashService::FindGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	auto FindByName = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
			{
				return Graph;
			}
		}
		return nullptr;
	};

	if (UEdGraph* Graph = FindByName(Blueprint->UbergraphPages))
	{
		return Graph;
	}
	if (UEdGraph* Graph = FindByName(Blueprint->FunctionGraphs))
	{
		return Graph;
	}
	if (UEdGraph* Graph = FindByName(Blueprint->MacroGraphs))
	{
		return Graph;
	}
	return nullptr;
}

UEdGraphNode* FBlueprintHelperReviewHashService::FindNodeByName(UEdGraph* Graph, const FString& NodeName)
{
	if (!Graph || NodeName.IsEmpty())
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetName() == NodeName)
		{
			return Node;
		}
	}
	return nullptr;
}

static UEdGraphNode* FindReviewNodeByGuid(UEdGraph* Graph, const FString& NodeGuid)
{
	if (!Graph || NodeGuid.IsEmpty())
	{
		return nullptr;
	}

	FGuid ParsedGuid;
	const bool bParsedGuid = FGuid::Parse(NodeGuid, ParsedGuid);
	const FString NormalizedNodeGuid = NodeGuid.Replace(TEXT("-"), TEXT(""));
	if (!bParsedGuid && NormalizedNodeGuid.IsEmpty())
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (bParsedGuid && Node->NodeGuid == ParsedGuid)
		{
			return Node;
		}
		if (Node->NodeGuid.ToString(EGuidFormats::Digits) == NormalizedNodeGuid)
		{
			return Node;
		}
	}
	return nullptr;
}

static bool IsReviewIgnoredGraphNode(const UEdGraphNode* Node)
{
	if (!Node || !Node->GetClass())
	{
		return false;
	}

	const FString ClassName = Node->GetClass()->GetName();
	return ClassName.Contains(TEXT("Comment"))
		|| ClassName.Contains(TEXT("K2Node_Knot"))
		|| ClassName.Contains(TEXT("Knot"));
}

static FString GetReviewNodeMetadataValue(const UEdGraphNode* Node, const TCHAR* Key)
{
	if (!Node || !Key)
	{
		return FString();
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		return FString();
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	return MetaData.GetValue(Node, Key);
}

static FString MakeReviewNodeLocalSemanticIdentity(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}

	const TCHAR* StableMetadataKeys[] = {
		TEXT("BlueprintHelperNodeId"),
		TEXT("BlueprintHelperFragmentId"),
		TEXT("BlueprintHelperStatementId"),
	};
	for (const TCHAR* Key : StableMetadataKeys)
	{
		const FString Value = GetReviewNodeMetadataValue(Node, Key);
		if (!Value.IsEmpty())
		{
			return FString::Printf(TEXT("meta:%s=%s"), Key, *Value);
		}
	}

	TArray<FString> PinParts;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		PinParts.Add(FString::Printf(
			TEXT("%s|%d|%s|%s|%s"),
			*Pin->PinName.ToString(),
			static_cast<int32>(Pin->Direction),
			*Pin->PinType.PinCategory.ToString(),
			*Pin->PinType.PinSubCategory.ToString(),
			*Pin->DefaultValue));
	}
	PinParts.Sort();

	const FString NodeClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
	return FBlueprintHelperReviewHashService::MakeStableHash(FString::Printf(
		TEXT("node_local_semantic|%s|%s"),
		*NodeClassPath,
		*FString::Join(PinParts, TEXT(";"))));
}

static void CollectReviewSemanticLinkedPins(
	const UEdGraphPin* SourcePin,
	const UEdGraphPin* SearchPin,
	TSet<const UEdGraphPin*>& VisitedPins,
	TArray<FString>& OutLinkParts,
	bool bUseStableSemanticIdentity)
{
	if (!SearchPin || VisitedPins.Contains(SearchPin))
	{
		return;
	}
	VisitedPins.Add(SearchPin);

	for (const UEdGraphPin* LinkedPin : SearchPin->LinkedTo)
	{
		if (!LinkedPin || LinkedPin == SourcePin)
		{
			continue;
		}

		const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
		if (!LinkedNode)
		{
			continue;
		}

		if (IsReviewIgnoredGraphNode(LinkedNode))
		{
			for (const UEdGraphPin* BridgePin : LinkedNode->Pins)
			{
				if (BridgePin && BridgePin != LinkedPin)
				{
					CollectReviewSemanticLinkedPins(SourcePin, BridgePin, VisitedPins, OutLinkParts, bUseStableSemanticIdentity);
				}
			}
			continue;
		}

		const FString LinkedNodeIdentity = bUseStableSemanticIdentity
			? MakeReviewNodeLocalSemanticIdentity(LinkedNode)
			: LinkedNode->NodeGuid.ToString(EGuidFormats::Digits);
		OutLinkParts.Add(FString::Printf(
			TEXT("%s.%s.%d"),
			*LinkedNodeIdentity,
			*LinkedPin->PinName.ToString(),
			static_cast<int32>(LinkedPin->Direction)));
	}
}

FString FBlueprintHelperReviewHashService::ExtractAnchorName(
	const FString& TargetKey,
	const FString& Prefix)
{
	const FString Marker = Prefix + TEXT(":");
	const int32 MarkerPos = TargetKey.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (MarkerPos != INDEX_NONE)
	{
		return TargetKey.Mid(MarkerPos + Marker.Len());
	}

	int32 LastColon = INDEX_NONE;
	if (TargetKey.FindLastChar(TEXT(':'), LastColon))
	{
		return TargetKey.Mid(LastColon + 1);
	}
	return TargetKey;
}

FString FBlueprintHelperReviewHashService::ComputeNodeHash(UEdGraphNode* Node, bool bUseStableSemanticIdentity)
{
	if (!Node)
	{
		return FString();
	}

	TArray<FString> PinParts;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		TArray<FString> LinkParts;
		TSet<const UEdGraphPin*> VisitedPins;
		CollectReviewSemanticLinkedPins(Pin, Pin, VisitedPins, LinkParts, bUseStableSemanticIdentity);
		LinkParts.Sort();

		PinParts.Add(FString::Printf(
			TEXT("%s|%d|%s|%s|%s|%s"),
			*Pin->PinName.ToString(),
			static_cast<int32>(Pin->Direction),
			*Pin->PinType.PinCategory.ToString(),
			*Pin->PinType.PinSubCategory.ToString(),
			*Pin->DefaultValue,
			*FString::Join(LinkParts, TEXT(","))));
	}
	PinParts.Sort();

	const FString NodeClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
	const FString NodeIdentity = bUseStableSemanticIdentity
		? MakeReviewNodeLocalSemanticIdentity(Node)
		: Node->NodeGuid.ToString(EGuidFormats::Digits);
	const FString Payload = FString::Printf(
		TEXT("node|%s|%s|%s"),
		*NodeClassPath,
		*NodeIdentity,
		*FString::Join(PinParts, TEXT(";")));
	return MakeStableHash(Payload);
}

bool FBlueprintHelperReviewHashService::ComputeGraphNodeHash(
	UBlueprint* Blueprint,
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutHash,
	FString& OutError)
{
	UEdGraph* Graph = FindGraph(Blueprint, Target.GraphName);
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
		return false;
	}

	UEdGraphNode* Node = FindReviewNodeByGuid(Graph, Target.NodeGuid);
	const FString NodeName = ExtractAnchorName(Target.TargetKey, TEXT("node"));
	if (!Node)
	{
		Node = FindNodeByName(Graph, NodeName);
	}
	if (!Node || IsReviewIgnoredGraphNode(Node))
	{
		OutError = FString::Printf(
			TEXT("%s:%s"),
			Node ? TEXT("node_ignored") : TEXT("node_not_found"),
			Target.NodeGuid.IsEmpty() ? *NodeName : *Target.NodeGuid);
		return false;
	}

	OutHash = ComputeNodeHash(Node);
	return !OutHash.IsEmpty();
}

bool FBlueprintHelperReviewHashService::ComputeGraphBlockHash(
	UBlueprint* Blueprint,
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutHash,
	FString& OutError)
{
	UEdGraph* Graph = FindGraph(Blueprint, Target.GraphName);
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
		return false;
	}

	const FString BlockId = ExtractAnchorName(Target.TargetKey, TEXT("block"));
	TArray<FString> NodeHashes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || IsReviewIgnoredGraphNode(Node))
		{
			continue;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			continue;
		}
		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		if (NodeBlockId == BlockId)
		{
			NodeHashes.Add(ComputeNodeHash(Node, true));
		}
	}

	if (NodeHashes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("block_not_found:%s"), *BlockId);
		return false;
	}

	NodeHashes.Sort();
	OutHash = MakeStableHash(FString::Printf(TEXT("block|%s|%s"), *BlockId, *FString::Join(NodeHashes, TEXT(";"))));
	return true;
}

bool FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutHash,
	FString& OutError)
{
	OutHash.Reset();
	OutError.Reset();

	UBlueprint* Blueprint = LoadBlueprint(Target.AssetPath, OutError);
	if (!Blueprint)
	{
		return false;
	}

	if (Target.TargetKind == TEXT("graph_node") || Target.TargetKey.Contains(TEXT(":node:")))
	{
		return ComputeGraphNodeHash(Blueprint, Target, OutHash, OutError);
	}
	if (Target.TargetKind == TEXT("graph_block") || Target.TargetKey.Contains(TEXT(":block:")))
	{
		return ComputeGraphBlockHash(Blueprint, Target, OutHash, OutError);
	}

	OutError = FString::Printf(TEXT("hash_unsupported_target_kind:%s"), *Target.TargetKind);
	return false;
}
