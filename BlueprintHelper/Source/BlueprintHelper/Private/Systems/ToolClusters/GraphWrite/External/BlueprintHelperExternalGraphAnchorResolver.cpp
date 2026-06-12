#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h"

namespace BlueprintHelperExternalGraphAnchorResolver
{
	static bool IsSchemaSupported(const FBlueprintHelperExternalGraphAnchor& Anchor)
	{
		return Anchor.Schema == FBlueprintHelperExternalGraphAnchor::SchemaString;
	}

	static FString DirectionToString(const EEdGraphPinDirection Direction)
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

	static UBlueprint* FindBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		if (UBlueprint* LoadedBlueprint = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return LoadedBlueprint;
		}

		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static void AddGraphIfValid(TArray<UEdGraph*>& Graphs, UEdGraph* Graph)
	{
		if (Graph)
		{
			Graphs.Add(Graph);
		}
	}

	static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint || GraphName.IsEmpty())
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid)
	{
		if (!Graph || NodeGuid.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid.ToString(EGuidFormats::Digits).Equals(NodeGuid, ESearchCase::IgnoreCase))
			{
				return Node;
			}
		}
		return nullptr;
	}

	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FBlueprintHelperExternalGraphAnchor& Anchor)
	{
		if (!Node || Anchor.PinName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			if (Pin->PinName.ToString().Equals(Anchor.PinName, ESearchCase::IgnoreCase)
				&& DirectionToString(Pin->Direction).Equals(Anchor.PinDirection, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static FString CompactNodeKey(const UEdGraphNode* Node)
	{
		const FString Guid = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		return Guid.Len() <= 8 ? Guid : Guid.Left(8);
	}

	static FString CompactPinKey(const UEdGraphPin* Pin)
	{
		FString Cleaned;
		const FString Raw = Pin ? Pin->PinName.ToString() : FString();
		Cleaned.Reserve(Raw.Len());
		for (const TCHAR Ch : Raw)
		{
			if (FChar::IsAlnum(Ch) || Ch == TCHAR('_'))
			{
				Cleaned.AppendChar(Ch);
			}
		}
		if (Cleaned.Len() > 16)
		{
			Cleaned = Cleaned.Left(16);
		}
		return Cleaned.IsEmpty() ? FString(TEXT("pin")) : Cleaned;
	}

	static FString CompactLinkKindToString(EBlueprintHelperExternalCompactLinkKind Kind)
	{
		if (Kind == EBlueprintHelperExternalCompactLinkKind::Exec)
		{
			return TEXT("exec");
		}
		if (Kind == EBlueprintHelperExternalCompactLinkKind::Data)
		{
			return TEXT("data");
		}
		return TEXT("unknown");
	}

	static bool ResolveCompactNode(
		UEdGraph* Graph,
		const FString& NodeKey,
		UEdGraphNode*& OutNode,
		FString& OutError)
	{
		OutNode = nullptr;
		if (!Graph || NodeKey.IsEmpty())
		{
			OutError = TEXT("external_anchor_ref_invalid");
			return false;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || !CompactNodeKey(Node).Equals(NodeKey, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (OutNode)
			{
				OutError = TEXT("external_anchor_ambiguous");
				OutNode = nullptr;
				return false;
			}
			OutNode = Node;
		}

		if (!OutNode)
		{
			OutError = TEXT("external_anchor_node_not_found");
			return false;
		}
		return true;
	}

	static bool ResolveCompactPinOnNode(
		UEdGraphNode* Node,
		const FString& PinKey,
		UEdGraphPin*& OutPin,
		FString& OutError)
	{
		OutPin = nullptr;
		if (!Node || PinKey.IsEmpty())
		{
			OutError = TEXT("external_anchor_ref_invalid");
			return false;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || !CompactPinKey(Pin).Equals(PinKey, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (OutPin)
			{
				OutError = TEXT("external_anchor_ambiguous");
				OutPin = nullptr;
				return false;
			}
			OutPin = Pin;
		}

		if (!OutPin)
		{
			OutError = TEXT("external_anchor_pin_not_found");
			return false;
		}
		return true;
	}
}

bool FBlueprintHelperExternalGraphAnchorResolver::ResolveNode(
	const FBlueprintHelperExternalGraphAnchor& Anchor,
	UEdGraphNode*& OutNode,
	FString& OutError) const
{
	OutNode = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::IsSchemaSupported(Anchor))
	{
		OutError = TEXT("external_anchor_schema_unsupported");
		return false;
	}

	UBlueprint* Blueprint = BlueprintHelperExternalGraphAnchorResolver::FindBlueprint(Anchor.AssetPath);
	UEdGraph* Graph = BlueprintHelperExternalGraphAnchorResolver::FindGraph(Blueprint, Anchor.GraphName);
	if (!Graph)
	{
		OutError = TEXT("external_anchor_graph_not_found");
		return false;
	}

	UEdGraphNode* Node = BlueprintHelperExternalGraphAnchorResolver::FindNodeByGuid(Graph, Anchor.NodeGuid);
	if (!Node)
	{
		OutError = TEXT("external_anchor_node_not_found");
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	if (!FingerprintService.BuildNodeFingerprint(Node).Equals(Anchor.Fingerprint, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_fingerprint_mismatch");
		return false;
	}

	OutNode = Node;
	return true;
}

bool FBlueprintHelperExternalGraphAnchorResolver::ResolvePin(
	const FBlueprintHelperExternalGraphAnchor& Anchor,
	UEdGraphPin*& OutPin,
	FString& OutError) const
{
	OutPin = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::IsSchemaSupported(Anchor))
	{
		OutError = TEXT("external_anchor_schema_unsupported");
		return false;
	}

	UBlueprint* Blueprint = BlueprintHelperExternalGraphAnchorResolver::FindBlueprint(Anchor.AssetPath);
	UEdGraph* Graph = BlueprintHelperExternalGraphAnchorResolver::FindGraph(Blueprint, Anchor.GraphName);
	if (!Graph)
	{
		OutError = TEXT("external_anchor_graph_not_found");
		return false;
	}

	UEdGraphNode* Node = BlueprintHelperExternalGraphAnchorResolver::FindNodeByGuid(Graph, Anchor.NodeGuid);
	if (!Node)
	{
		OutError = TEXT("external_anchor_node_not_found");
		return false;
	}

	UEdGraphPin* Pin = BlueprintHelperExternalGraphAnchorResolver::FindPin(Node, Anchor);
	if (!Pin)
	{
		OutError = TEXT("external_anchor_pin_not_found");
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	const FString CurrentFingerprint = Anchor.SemanticRole == EBlueprintHelperExternalGraphAnchorRole::ExecBoundary
		? FingerprintService.BuildExecBoundaryFingerprint(Pin)
		: FingerprintService.BuildPinFingerprint(Pin);
	if (!CurrentFingerprint.Equals(Anchor.Fingerprint, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_fingerprint_mismatch");
		return false;
	}

	OutPin = Pin;
	return true;
}

bool FBlueprintHelperExternalGraphAnchorResolver::ResolveCompactPin(
	const FString& AssetPath,
	const FString& GraphName,
	const FBlueprintHelperExternalCompactAnchor& Anchor,
	UEdGraphPin*& OutPin,
	FString& OutError) const
{
	OutPin = nullptr;
	if (Anchor.Type != EBlueprintHelperExternalCompactAnchorType::Pin)
	{
		OutError = TEXT("external_anchor_ref_unsupported");
		return false;
	}

	UBlueprint* Blueprint = BlueprintHelperExternalGraphAnchorResolver::FindBlueprint(AssetPath);
	UEdGraph* Graph = BlueprintHelperExternalGraphAnchorResolver::FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		OutError = Blueprint ? TEXT("external_anchor_graph_not_found") : TEXT("external_anchor_blueprint_not_found");
		return false;
	}

	UEdGraphNode* Node = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactNode(Graph, Anchor.NodeKey, Node, OutError))
	{
		return false;
	}

	UEdGraphPin* Pin = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactPinOnNode(Node, Anchor.PinKey, Pin, OutError))
	{
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	if (!FingerprintService.BuildCompactPinFingerprint(Pin).Equals(Anchor.Fingerprint, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_stale");
		return false;
	}

	OutPin = Pin;
	return true;
}

bool FBlueprintHelperExternalGraphAnchorResolver::ResolveCompactNode(
	const FString& AssetPath,
	const FString& GraphName,
	const FBlueprintHelperExternalCompactAnchor& Anchor,
	UEdGraphNode*& OutNode,
	FString& OutError) const
{
	OutNode = nullptr;
	if (Anchor.Type != EBlueprintHelperExternalCompactAnchorType::Node)
	{
		OutError = TEXT("external_anchor_ref_unsupported");
		return false;
	}

	UBlueprint* Blueprint = BlueprintHelperExternalGraphAnchorResolver::FindBlueprint(AssetPath);
	UEdGraph* Graph = BlueprintHelperExternalGraphAnchorResolver::FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		OutError = Blueprint ? TEXT("external_anchor_graph_not_found") : TEXT("external_anchor_blueprint_not_found");
		return false;
	}

	UEdGraphNode* Node = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactNode(Graph, Anchor.NodeKey, Node, OutError))
	{
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	if (!FingerprintService.BuildCompactNodeFingerprint(Node).Equals(Anchor.Fingerprint, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_stale");
		return false;
	}

	OutNode = Node;
	return true;
}

bool FBlueprintHelperExternalGraphAnchorResolver::ResolveCompactLink(
	const FString& AssetPath,
	const FString& GraphName,
	const FBlueprintHelperExternalCompactAnchor& Anchor,
	FBlueprintHelperExternalGraphLinkResolution& OutLink,
	FString& OutError) const
{
	OutLink = FBlueprintHelperExternalGraphLinkResolution();
	if (Anchor.Type != EBlueprintHelperExternalCompactAnchorType::Link)
	{
		OutError = TEXT("external_anchor_ref_unsupported");
		return false;
	}

	UBlueprint* Blueprint = BlueprintHelperExternalGraphAnchorResolver::FindBlueprint(AssetPath);
	UEdGraph* Graph = BlueprintHelperExternalGraphAnchorResolver::FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		OutError = Blueprint ? TEXT("external_anchor_graph_not_found") : TEXT("external_anchor_blueprint_not_found");
		return false;
	}

	UEdGraphNode* SourceNode = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactNode(Graph, Anchor.SourceNodeKey, SourceNode, OutError))
	{
		return false;
	}
	UEdGraphNode* TargetNode = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactNode(Graph, Anchor.TargetNodeKey, TargetNode, OutError))
	{
		return false;
	}

	UEdGraphPin* SourcePin = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactPinOnNode(SourceNode, Anchor.SourcePinKey, SourcePin, OutError))
	{
		return false;
	}
	UEdGraphPin* TargetPin = nullptr;
	if (!BlueprintHelperExternalGraphAnchorResolver::ResolveCompactPinOnNode(TargetNode, Anchor.TargetPinKey, TargetPin, OutError))
	{
		return false;
	}

	if (!SourcePin->LinkedTo.Contains(TargetPin))
	{
		OutError = TEXT("external_link_not_found");
		return false;
	}

	const FString LinkKind = BlueprintHelperExternalGraphAnchorResolver::CompactLinkKindToString(Anchor.LinkKind);
	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	const FString CurrentFingerprint = FingerprintService.BuildLinkFingerprint(SourcePin, TargetPin, LinkKind);
	if (!CurrentFingerprint.Equals(Anchor.Fingerprint, ESearchCase::IgnoreCase))
	{
		OutError = TEXT("external_anchor_stale");
		return false;
	}

	OutLink.SourcePin = SourcePin;
	OutLink.TargetPin = TargetPin;
	OutLink.LinkKind = LinkKind;
	return true;
}
