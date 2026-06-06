#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CustomEvent.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

class FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils
{
public:
	static bool TryParseIndexedRef(const FString& Ref, const TCHAR* Prefix, int32& OutIndex)
	{
		OutIndex = INDEX_NONE;
		if (Ref.IsEmpty())
		{
			return false;
		}

		const FString PrefixString(Prefix);
		const int32 PrefixIndex = Ref.Find(PrefixString, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (PrefixIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 NumberStart = PrefixIndex + PrefixString.Len();
		const int32 CloseIndex = Ref.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, NumberStart);
		if (CloseIndex == INDEX_NONE || CloseIndex <= NumberStart)
		{
			return false;
		}

		const FString IndexText = Ref.Mid(NumberStart, CloseIndex - NumberStart);
		if (!IndexText.IsNumeric())
		{
			return false;
		}

		OutIndex = FCString::Atoi(*IndexText);
		return true;
	}

	static bool TryResolveNodeGuidInSet(const TArray<UEdGraphNode*>& Nodes, const FString& Ref, UEdGraphNode*& OutNode)
	{
		if (Ref.IsEmpty())
		{
			return false;
		}

		FGuid Guid;
		if (!FGuid::ParseExact(Ref, EGuidFormats::Digits, Guid) &&
			!FGuid::ParseExact(Ref, EGuidFormats::DigitsWithHyphens, Guid))
		{
			return false;
		}

		for (UEdGraphNode* Node : Nodes)
		{
			if (Node && Node->NodeGuid == Guid)
			{
				OutNode = Node;
				return true;
			}
		}

		return false;
	}

	static bool IsNodeOwnedByBlock(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node || BlockId.IsEmpty())
		{
			return false;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
			const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
			if (!NodeBlockId.IsEmpty() || !OwnedStr.IsEmpty())
			{
				return NodeBlockId == BlockId && OwnedStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);
			}
		}

		return false;
	}

	static void CollectBlockNodes(UEdGraph* Graph, const FString& BlockId, TArray<UEdGraphNode*>& OutNodes)
	{
		OutNodes.Reset();
		if (!Graph || BlockId.IsEmpty())
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (IsNodeOwnedByBlock(Node, BlockId))
			{
				OutNodes.Add(Node);
			}
		}
	}

	static bool ResolveIndexedNode(const TArray<UEdGraphNode*>& BlockNodes, const FString& Ref, UEdGraphNode*& OutNode)
	{
		int32 NodeIndex = INDEX_NONE;
		if (!TryParseIndexedRef(Ref, TEXT("nodes["), NodeIndex))
		{
			return false;
		}

		if (BlockNodes.IsValidIndex(NodeIndex) && BlockNodes[NodeIndex])
		{
			OutNode = BlockNodes[NodeIndex];
			return true;
		}

		return false;
	}

	static bool ResolveNamedNodeInBlock(const TArray<UEdGraphNode*>& BlockNodes, const FString& Ref, UEdGraphNode*& OutNode, bool& bOutAmbiguous)
	{
		bOutAmbiguous = false;
		if (Ref.IsEmpty())
		{
			return false;
		}

		TArray<UEdGraphNode*> Matches;
		for (UEdGraphNode* Node : BlockNodes)
		{
			if (!Node)
			{
				continue;
			}

			const FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			const UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node);
			const bool bMatchesCustomEventName = CustomEventNode &&
				CustomEventNode->CustomFunctionName.ToString().Equals(Ref, ESearchCase::IgnoreCase);
			if (Node->GetName().Equals(Ref, ESearchCase::IgnoreCase) ||
				Title.Equals(Ref, ESearchCase::IgnoreCase) ||
				bMatchesCustomEventName)
			{
				Matches.Add(Node);
			}
		}

		if (Matches.Num() == 1)
		{
			OutNode = Matches[0];
			return true;
		}

		bOutAmbiguous = Matches.Num() > 1;
		return false;
	}

	static bool ResolveNodeTokenInBlock(const TArray<UEdGraphNode*>& BlockNodes, const FString& Ref, UEdGraphNode*& OutNode, bool& bOutAmbiguous)
	{
		bOutAmbiguous = false;
		if (ResolveIndexedNode(BlockNodes, Ref, OutNode) ||
			TryResolveNodeGuidInSet(BlockNodes, Ref, OutNode))
		{
			return true;
		}
		return ResolveNamedNodeInBlock(BlockNodes, Ref, OutNode, bOutAmbiguous);
	}

	static bool ResolvePinToken(UEdGraphNode* Node, const FString& Ref, UEdGraphPin*& OutPin, bool& bOutAmbiguous)
	{
		OutPin = nullptr;
		bOutAmbiguous = false;
		if (!Node || Ref.IsEmpty())
		{
			return false;
		}

		TArray<UEdGraphPin*> Matches;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString().Equals(Ref, ESearchCase::IgnoreCase))
			{
				Matches.Add(Pin);
			}
		}

		if (Matches.Num() == 1)
		{
			OutPin = Matches[0];
			return true;
		}

		bOutAmbiguous = Matches.Num() > 1;
		return false;
	}

	static bool ResolveLinkPathInBlock(
		const TArray<UEdGraphNode*>& BlockNodes,
		const FString& LinkPath,
		FBlueprintHelperResolvedLink& OutLink,
		FBlueprintHelperPatchResolveError& OutError)
	{
		if (LinkPath.IsEmpty())
		{
			return false;
		}

		int32 ArrowPos = INDEX_NONE;
		if (!LinkPath.FindChar(TEXT('>'), ArrowPos) || ArrowPos <= 0 || ArrowPos >= LinkPath.Len() - 1)
		{
			return false;
		}

		const FString FromPart = LinkPath.Left(ArrowPos - 1);
		const FString ToPart = LinkPath.Mid(ArrowPos + 1);

		FString FromNodeRef;
		FString FromPinRef;
		FString ToNodeRef;
		FString ToPinRef;
		int32 FromDot = INDEX_NONE;
		int32 ToDot = INDEX_NONE;
		if (!FromPart.FindLastChar(TEXT('.'), FromDot) || !ToPart.FindLastChar(TEXT('.'), ToDot))
		{
			return false;
		}

		FromNodeRef = FromPart.Left(FromDot);
		FromPinRef = FromPart.Mid(FromDot + 1);
		ToNodeRef = ToPart.Left(ToDot);
		ToPinRef = ToPart.Mid(ToDot + 1);

		bool bAmbiguous = false;
		UEdGraphNode* SourceNode = nullptr;
		UEdGraphNode* TargetNode = nullptr;
		if (!ResolveNodeTokenInBlock(BlockNodes, FromNodeRef, SourceNode, bAmbiguous))
		{
			OutError = {bAmbiguous ? TEXT("target_ambiguous") : TEXT("target_link_not_found"),
				FString::Printf(TEXT("Unable to resolve source node '%s' inside owned block link_path '%s'."),
					*FromNodeRef, *LinkPath),
				LinkPath};
			return false;
		}
		if (!ResolveNodeTokenInBlock(BlockNodes, ToNodeRef, TargetNode, bAmbiguous))
		{
			OutError = {bAmbiguous ? TEXT("target_ambiguous") : TEXT("target_link_not_found"),
				FString::Printf(TEXT("Unable to resolve target node '%s' inside owned block link_path '%s'."),
					*ToNodeRef, *LinkPath),
				LinkPath};
			return false;
		}

		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		if (!ResolvePinToken(SourceNode, FromPinRef, SourcePin, bAmbiguous))
		{
			OutError = {bAmbiguous ? TEXT("target_ambiguous") : TEXT("target_link_not_found"),
				FString::Printf(TEXT("Unable to resolve source pin '%s' inside owned block link_path '%s'."),
					*FromPinRef, *LinkPath),
				LinkPath};
			return false;
		}
		if (!ResolvePinToken(TargetNode, ToPinRef, TargetPin, bAmbiguous))
		{
			OutError = {bAmbiguous ? TEXT("target_ambiguous") : TEXT("target_link_not_found"),
				FString::Printf(TEXT("Unable to resolve target pin '%s' inside owned block link_path '%s'."),
					*ToPinRef, *LinkPath),
				LinkPath};
			return false;
		}

		if (!SourcePin->LinkedTo.Contains(TargetPin))
		{
			OutError = {TEXT("target_link_not_found"),
				FString::Printf(TEXT("Owned block link_path '%s' does not describe an existing link."), *LinkPath),
				LinkPath};
			return false;
		}

		OutLink.SourceNode = SourceNode;
		OutLink.SourcePin = SourcePin;
		OutLink.TargetNode = TargetNode;
		OutLink.TargetPin = TargetPin;
		OutLink.LinkRef = LinkPath;
		return true;
	}

	static int32 ResolveLinkRefMatchesInBlock(
		const TArray<UEdGraphNode*>& BlockNodes,
		const FString& LinkRef,
		FBlueprintHelperResolvedLink& OutLink)
	{
		if (LinkRef.IsEmpty())
		{
			return 0;
		}

		int32 MatchCount = 0;
		for (UEdGraphNode* Node : BlockNodes)
		{
			if (!Node)
			{
				continue;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!LinkedNode || !BlockNodes.Contains(LinkedNode))
					{
						continue;
					}

					const FString Candidate = FString::Printf(TEXT("%s.%s->%s.%s"),
						*Node->GetName(),
						*Pin->PinName.ToString(),
						*LinkedNode->GetName(),
						*LinkedPin->PinName.ToString());
					if (Candidate == LinkRef)
					{
						++MatchCount;
						if (MatchCount == 1)
						{
							OutLink.SourceNode = Node;
							OutLink.SourcePin = Pin;
							OutLink.TargetNode = LinkedNode;
							OutLink.TargetPin = LinkedPin;
							OutLink.LinkRef = LinkRef;
						}
					}
				}
			}
		}
		return MatchCount;
	}

	static bool BlockContainsNode(const TArray<UEdGraphNode*>& BlockNodes, UEdGraphNode* Node)
	{
		return Node && BlockNodes.Contains(Node);
	}

};

bool FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(
	const FBlueprintHelperLogicJsonPathService& PathService,
	UEdGraph* Graph,
	const FBlueprintHelperGraphWriteAnchorRef& Anchor,
	UEdGraphNode*& OutNode,
	FBlueprintHelperPatchResolveError& OutError)
{
	OutNode = nullptr;
	(void)PathService;

	if (!Anchor.IsBlockScoped())
	{
		OutError = {TEXT("owned_block_anchor_required"),
			TEXT("GraphWrite owned node resolution requires anchor.block_id."),
			TEXT("target_ref.block_id")};
		return false;
	}

	if (!Graph)
	{
		OutError = {TEXT("target_graph_not_found"), TEXT("Graph is null."), TEXT("graph")};
		return false;
	}

	TArray<UEdGraphNode*> BlockNodes;
	FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::CollectBlockNodes(Graph, Anchor.BlockId, BlockNodes);
	if (BlockNodes.Num() == 0)
	{
		OutError = {TEXT("target_group_not_found"),
			FString::Printf(TEXT("BlueprintHelper block '%s' was not found in graph '%s'."),
				*Anchor.BlockId, *Graph->GetName()),
			Anchor.BlockId};
		return false;
	}

	const FString EffectiveNodePath = !Anchor.NodePath.IsEmpty()
		? Anchor.NodePath
		: (Anchor.NodeRef.IsEmpty() ? Anchor.GroupEntryNodePath : FString());
	if (!EffectiveNodePath.IsEmpty())
	{
		if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveIndexedNode(BlockNodes, EffectiveNodePath, OutNode))
		{
			return true;
		}

		bool bAmbiguous = false;
		if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveNamedNodeInBlock(BlockNodes, EffectiveNodePath, OutNode, bAmbiguous))
		{
			return true;
		}
		if (bAmbiguous)
		{
			OutError = {TEXT("target_ambiguous"),
				FString::Printf(TEXT("node_path '%s' matched multiple nodes in block '%s'."),
					*EffectiveNodePath, *Anchor.BlockId),
				EffectiveNodePath};
			return false;
		}
	}

	if (!Anchor.NodeRef.IsEmpty())
	{
		if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveIndexedNode(BlockNodes, Anchor.NodeRef, OutNode))
		{
			return true;
		}

		if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::TryResolveNodeGuidInSet(BlockNodes, Anchor.NodeRef, OutNode))
		{
			return true;
		}

		bool bAmbiguous = false;
		if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveNamedNodeInBlock(BlockNodes, Anchor.NodeRef, OutNode, bAmbiguous))
		{
			return true;
		}
		if (bAmbiguous)
		{
			OutError = {TEXT("target_ambiguous"),
				FString::Printf(TEXT("node_ref '%s' matched multiple nodes in block '%s'."),
					*Anchor.NodeRef, *Anchor.BlockId),
				Anchor.NodeRef};
			return false;
		}
	}

	OutError = {TEXT("target_node_not_found"),
		FString::Printf(TEXT("Unable to resolve node_ref '%s' inside BlueprintHelper block '%s'."),
			*Anchor.NodeRef, *Anchor.BlockId),
		Anchor.NodeRef};
	return false;
}

bool FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(
	const FBlueprintHelperLogicJsonPathService& PathService,
	UEdGraph* Graph,
	UEdGraphNode* OwningNode,
	const FBlueprintHelperGraphWriteAnchorRef& Anchor,
	UEdGraphPin*& OutPin,
	FBlueprintHelperPatchResolveError& OutError)
{
	return PathService.ResolvePin(Graph, OwningNode, Anchor.PinRef, Anchor.PinPath, OutPin, OutError);
}

bool FBlueprintHelperGraphWriteBlockScopedResolver::ResolveLink(
	const FBlueprintHelperLogicJsonPathService& PathService,
	UEdGraph* Graph,
	const FBlueprintHelperGraphWriteAnchorRef& Anchor,
	FBlueprintHelperResolvedLink& OutLink,
	FBlueprintHelperPatchResolveError& OutError)
{
	if (!Anchor.IsBlockScoped())
	{
		OutError = {TEXT("owned_block_anchor_required"),
			TEXT("GraphWrite owned link resolution requires anchor.block_id."),
			TEXT("target_ref.block_id")};
		return false;
	}

	TArray<UEdGraphNode*> BlockNodes;
	FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::CollectBlockNodes(Graph, Anchor.BlockId, BlockNodes);
	if (BlockNodes.Num() == 0)
	{
		OutError = {TEXT("target_group_not_found"),
			FString::Printf(TEXT("BlueprintHelper block '%s' was not found in graph '%s'."),
				*Anchor.BlockId, Graph ? *Graph->GetName() : TEXT("?")),
			Anchor.BlockId};
		return false;
	}

	int32 LinkIndex = INDEX_NONE;
	if (FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::TryParseIndexedRef(Anchor.LinkRef, TEXT("links["), LinkIndex))
	{
		TArray<UEdGraphNode*> SourceNodes;
		if (!Anchor.NodeRef.IsEmpty() || !Anchor.NodePath.IsEmpty())
		{
			UEdGraphNode* SourceNode = nullptr;
			FBlueprintHelperPatchResolveError NodeError;
			if (!ResolveNode(PathService, Graph, Anchor, SourceNode, NodeError))
			{
				OutError = NodeError;
				return false;
			}
			SourceNodes.Add(SourceNode);
		}
		else
		{
			SourceNodes = BlockNodes;
		}

		int32 CurrentIndex = 0;
		for (UEdGraphNode* Node : SourceNodes)
		{
			if (!Node)
			{
				continue;
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output)
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::BlockContainsNode(BlockNodes, LinkedNode))
					{
						continue;
					}

					if (CurrentIndex == LinkIndex)
					{
						OutLink.SourceNode = Node;
						OutLink.SourcePin = Pin;
						OutLink.TargetNode = LinkedNode;
						OutLink.TargetPin = LinkedPin;
						OutLink.LinkRef = Anchor.LinkRef;
						return true;
					}
					++CurrentIndex;
				}
			}
		}

		OutError = {TEXT("target_link_not_found"),
			FString::Printf(TEXT("Unable to resolve link_ref '%s' inside BlueprintHelper block '%s'."),
				*Anchor.LinkRef, *Anchor.BlockId),
			Anchor.LinkRef};
		return false;
	}

	FBlueprintHelperPatchResolveError LinkPathError;
	if (!Anchor.LinkPath.IsEmpty() &&
		FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveLinkPathInBlock(
			BlockNodes,
			Anchor.LinkPath,
			OutLink,
			LinkPathError))
	{
		return true;
	}

	if (!Anchor.LinkRef.IsEmpty())
	{
		FBlueprintHelperPatchResolveError LinkRefPathError;
		if (Anchor.LinkRef.Contains(TEXT("->")) &&
			FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveLinkPathInBlock(
				BlockNodes,
				Anchor.LinkRef,
				OutLink,
				LinkRefPathError))
		{
			return true;
		}

		const int32 MatchCount = FBlueprintHelperGraphWriteBlockScopedResolverLocalUtils::ResolveLinkRefMatchesInBlock(
			BlockNodes,
			Anchor.LinkRef,
			OutLink);
		if (MatchCount == 1)
		{
			return true;
		}
		if (MatchCount > 1)
		{
			OutError = {TEXT("target_ambiguous"),
				FString::Printf(TEXT("link_ref '%s' matched multiple links inside BlueprintHelper block '%s'."),
					*Anchor.LinkRef, *Anchor.BlockId),
				Anchor.LinkRef};
			return false;
		}
	}

	OutError = !LinkPathError.Code.IsEmpty()
		? LinkPathError
		: FBlueprintHelperPatchResolveError{TEXT("target_link_not_found"),
			FString::Printf(TEXT("Unable to resolve link_ref '%s' inside BlueprintHelper block '%s'."),
				*Anchor.LinkRef, *Anchor.BlockId),
			Anchor.LinkRef};
	return false;
}
