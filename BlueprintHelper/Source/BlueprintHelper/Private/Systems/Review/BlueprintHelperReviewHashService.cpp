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

FString FBlueprintHelperReviewHashService::ComputeNodeHash(UEdGraphNode* Node)
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

		PinParts.Add(FString::Printf(
			TEXT("%s|%d|%s|%s|%s"),
			*Pin->PinName.ToString(),
			static_cast<int32>(Pin->Direction),
			*Pin->PinType.PinCategory.ToString(),
			*Pin->PinType.PinSubCategory.ToString(),
			*Pin->DefaultValue));
	}
	PinParts.Sort();

	const FString Payload = FString::Printf(
		TEXT("node|%s|%s|%s|%d|%d|%s"),
		*Node->GetName(),
		*Node->NodeGuid.ToString(EGuidFormats::Digits),
		*Node->NodeComment,
		Node->NodePosX,
		Node->NodePosY,
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

	const FString NodeName = Target.NodeGuid.IsEmpty()
		? ExtractAnchorName(Target.TargetKey, TEXT("node"))
		: Target.NodeGuid;
	UEdGraphNode* Node = FindNodeByName(Graph, NodeName);
	if (!Node)
	{
		OutError = FString::Printf(TEXT("node_not_found:%s"), *NodeName);
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
		if (!Node)
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
			NodeHashes.Add(ComputeNodeHash(Node));
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
