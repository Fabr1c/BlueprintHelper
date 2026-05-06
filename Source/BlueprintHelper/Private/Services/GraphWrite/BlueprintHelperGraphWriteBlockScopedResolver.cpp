#include "Services/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace
{
	bool TryParseIndexedRef(const FString& Ref, const TCHAR* Prefix, int32& OutIndex)
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

	bool TryResolveNodeGuidInSet(const TArray<UEdGraphNode*>& Nodes, const FString& Ref, UEdGraphNode*& OutNode)
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

	bool NodeCommentMentionsBlockId(const UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node || BlockId.IsEmpty() || !Node->NodeComment.Contains(TEXT("[BlueprintHelper]")))
		{
			return false;
		}

		TArray<FString> CommentLines;
		Node->NodeComment.ParseIntoArrayLines(CommentLines, false);
		for (const FString& Line : CommentLines)
		{
			FString Key;
			FString Value;
			if (!Line.Split(TEXT("="), &Key, &Value, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Key.TrimStartAndEnd().Equals(TEXT("block_id"), ESearchCase::IgnoreCase) &&
				Value.TrimStartAndEnd().Equals(BlockId, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}

		return false;
	}

	bool IsNodeOwnedByBlock(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node || BlockId.IsEmpty())
		{
			return false;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
			const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
			if (!NodeBlockId.IsEmpty() || !OwnedStr.IsEmpty())
			{
				return NodeBlockId == BlockId && OwnedStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);
			}
		}

		return NodeCommentMentionsBlockId(Node, BlockId);
	}

	void CollectBlockNodes(UEdGraph* Graph, const FString& BlockId, TArray<UEdGraphNode*>& OutNodes)
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

	bool ResolveIndexedNode(const TArray<UEdGraphNode*>& BlockNodes, const FString& Ref, UEdGraphNode*& OutNode)
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

	bool ResolveNamedNodeInBlock(const TArray<UEdGraphNode*>& BlockNodes, const FString& Ref, UEdGraphNode*& OutNode, bool& bOutAmbiguous)
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
			if (Node->GetName().Equals(Ref, ESearchCase::IgnoreCase) ||
				Title.Equals(Ref, ESearchCase::IgnoreCase))
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

	bool BlockContainsNode(const TArray<UEdGraphNode*>& BlockNodes, UEdGraphNode* Node)
	{
		return Node && BlockNodes.Contains(Node);
	}
}

bool FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(
	const FBlueprintHelperLogicJsonPathService& PathService,
	UEdGraph* Graph,
	const FBlueprintHelperGraphWriteAnchorRef& Anchor,
	UEdGraphNode*& OutNode,
	FBlueprintHelperPatchResolveError& OutError)
{
	OutNode = nullptr;

	if (!Anchor.IsBlockScoped())
	{
		return PathService.ResolveNode(Graph, Anchor.NodeRef, Anchor.NodePath, OutNode, OutError);
	}

	if (!Graph)
	{
		OutError = {TEXT("target_graph_not_found"), TEXT("Graph is null."), TEXT("graph")};
		return false;
	}

	TArray<UEdGraphNode*> BlockNodes;
	CollectBlockNodes(Graph, Anchor.BlockId, BlockNodes);
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
		if (ResolveIndexedNode(BlockNodes, EffectiveNodePath, OutNode))
		{
			return true;
		}

		bool bAmbiguous = false;
		if (ResolveNamedNodeInBlock(BlockNodes, EffectiveNodePath, OutNode, bAmbiguous))
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
		if (ResolveIndexedNode(BlockNodes, Anchor.NodeRef, OutNode))
		{
			return true;
		}

		if (TryResolveNodeGuidInSet(BlockNodes, Anchor.NodeRef, OutNode))
		{
			return true;
		}

		bool bAmbiguous = false;
		if (ResolveNamedNodeInBlock(BlockNodes, Anchor.NodeRef, OutNode, bAmbiguous))
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
		return PathService.ResolveLink(Graph, Anchor.LinkRef, Anchor.LinkPath, OutLink, OutError);
	}

	TArray<UEdGraphNode*> BlockNodes;
	CollectBlockNodes(Graph, Anchor.BlockId, BlockNodes);
	if (BlockNodes.Num() == 0)
	{
		OutError = {TEXT("target_group_not_found"),
			FString::Printf(TEXT("BlueprintHelper block '%s' was not found in graph '%s'."),
				*Anchor.BlockId, Graph ? *Graph->GetName() : TEXT("?")),
			Anchor.BlockId};
		return false;
	}

	int32 LinkIndex = INDEX_NONE;
	if (TryParseIndexedRef(Anchor.LinkRef, TEXT("links["), LinkIndex))
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
					if (!BlockContainsNode(BlockNodes, LinkedNode))
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

	FBlueprintHelperPatchResolveError LinkError;
	if (PathService.ResolveLink(Graph, Anchor.LinkRef, Anchor.LinkPath, OutLink, LinkError) &&
		BlockContainsNode(BlockNodes, OutLink.SourceNode) &&
		BlockContainsNode(BlockNodes, OutLink.TargetNode))
	{
		return true;
	}

	if (!LinkError.Code.IsEmpty())
	{
		OutError = LinkError;
	}
	else
	{
		OutError = {TEXT("target_link_not_found"),
			FString::Printf(TEXT("Unable to resolve link_ref '%s' inside BlueprintHelper block '%s'."),
				*Anchor.LinkRef, *Anchor.BlockId),
			Anchor.LinkRef};
	}
	return false;
}
