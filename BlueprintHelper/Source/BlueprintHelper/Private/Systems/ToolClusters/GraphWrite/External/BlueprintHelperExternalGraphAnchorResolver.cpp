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
