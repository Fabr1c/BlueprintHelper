#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReplaceCoordinator.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

class FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils
{
public:
	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && IsExecPin(Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool HasExecPin(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsExecPin(Pin))
			{
				return true;
			}
		}
		return false;
	}

	static bool HasInboundExecLinkFromImportedNode(
		UEdGraphPin* ExecInputPin,
		const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (!LinkedPin ||
				LinkedPin->Direction != EGPD_Output ||
				!IsExecPin(LinkedPin))
			{
				continue;
			}

			if (ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}
		return false;
	}

	static UEdGraphNode* FindFirstImportedExecutableBodyNode(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		TSet<UEdGraphNode*> ImportedNodes;
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || NodesBeforeImport.Contains(Node))
			{
				continue;
			}

			ImportedNodes.Add(Node);
			if (FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}
		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}

	static TArray<UEdGraphNode*> CollectImportedNodes(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedNodes;
		if (!Graph)
		{
			return ImportedNodes;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !NodesBeforeImport.Contains(Node))
			{
				ImportedNodes.Add(Node);
			}
		}
		return ImportedNodes;
	}

	static void CollectExecReachableFromBodyEntry(
		UEdGraphPin* BodyEntryPin,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		TSet<UEdGraphNode*>& OutReachable)
	{
		OutReachable.Empty();
		UEdGraphNode* BodyEntryNode = BodyEntryPin ? BodyEntryPin->GetOwningNode() : nullptr;
		if (!BodyEntryNode || !ImportedNodeSet.Contains(BodyEntryNode))
		{
			return;
		}

		TArray<UEdGraphNode*> PendingNodes;
		OutReachable.Add(BodyEntryNode);
		PendingNodes.Add(BodyEntryNode);

		while (PendingNodes.Num() > 0)
		{
			UEdGraphNode* Node = FBlueprintHelperVersionCompat::PopNoShrink(PendingNodes);
			if (!Node)
			{
				continue;
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode &&
						ImportedNodeSet.Contains(LinkedNode) &&
						HasExecPin(LinkedNode) &&
						!OutReachable.Contains(LinkedNode))
					{
						OutReachable.Add(LinkedNode);
						PendingNodes.Add(LinkedNode);
					}
				}
			}
		}
	}

	static void AddReachedExitBoundaries(
		const TArray<UEdGraphNode*>& ExitBoundaryNodes,
		TSet<UEdGraphNode*>& InOutBodyFlowNodes)
	{
		if (ExitBoundaryNodes.Num() == 0)
		{
			return;
		}

		bool bAddedNode = true;
		while (bAddedNode)
		{
			bAddedNode = false;
			for (UEdGraphNode* Node : ExitBoundaryNodes)
			{
				if (!Node || InOutBodyFlowNodes.Contains(Node))
				{
					continue;
				}

				UEdGraphPin* ExitExecInput = FindFirstExecPin(Node, EGPD_Input);
				if (!ExitExecInput)
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : ExitExecInput->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && InOutBodyFlowNodes.Contains(LinkedNode))
					{
						InOutBodyFlowNodes.Add(Node);
						bAddedNode = true;
						break;
					}
				}
			}
		}
	}

	static bool DataChainReachesReachableExecConsumer(
		const UEdGraphNode* Node,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes,
		TSet<const UEdGraphNode*>& VisitedNodes)
	{
		if (!Node || VisitedNodes.Contains(Node))
		{
			return false;
		}
		VisitedNodes.Add(Node);

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || LinkedPin->Direction != EGPD_Input || IsExecPin(LinkedPin))
				{
					continue;
				}

				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
				if (!LinkedNode || !ImportedNodeSet.Contains(LinkedNode))
				{
					continue;
				}

				if (HasExecPin(LinkedNode))
				{
					if (ReachableExecNodes.Contains(LinkedNode))
					{
						return true;
					}
					continue;
				}

				if (DataChainReachesReachableExecConsumer(LinkedNode, ImportedNodeSet, ReachableExecNodes, VisitedNodes))
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool GeneratedPureDataChainsReachBodyEntryExecFlow(
		const TArray<UEdGraphNode*>& ImportedNodes,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes)
	{
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (!Node || HasExecPin(Node))
			{
				continue;
			}

			TSet<const UEdGraphNode*> VisitedNodes;
			if (!DataChainReachesReachableExecConsumer(Node, ImportedNodeSet, ReachableExecNodes, VisitedNodes))
			{
				return false;
			}
		}
		return true;
	}

	static bool ImportedExecNodesReachBodyEntryExecFlow(
		const TArray<UEdGraphNode*>& ImportedNodes,
		const TSet<UEdGraphNode*>& ReachableExecNodes)
	{
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (HasExecPin(Node) && !ReachableExecNodes.Contains(Node))
			{
				return false;
			}
		}
		return true;
	}
};

bool FBlueprintHelperGraphBodyReplaceCoordinator::BuildPlan(
	const FBlueprintHelperGraphBodyRequest& Request,
	const IBlueprintHelperGraphBodyAdapter& Adapter,
	FBlueprintHelperGraphBodyReplacePlan& OutPlan,
	FString& OutError) const
{
	if (!Adapter.ResolveTarget(Request, OutPlan.Target, OutError))
	{
		return false;
	}
	OutPlan.BoundaryModel = Adapter.BuildBoundaryModel(OutPlan.Target, Request);
	OutPlan.MutationPlan = Adapter.BuildMutationPlan(OutPlan.Target, OutPlan.BoundaryModel, Request);
	OutPlan.SemanticContext = Adapter.BuildSemanticContext(OutPlan.Target, OutPlan.BoundaryModel);
	OutPlan.ReconnectPlan = Adapter.BuildReconnectPlan(OutPlan.Target, OutPlan.BoundaryModel);
	OutPlan.ConnectivityPolicy = Adapter.BuildConnectivityPolicy(OutPlan.Target, OutPlan.BoundaryModel);
	OutPlan.ReadbackProjection = Adapter.BuildReadbackProjection(OutPlan.Target, OutPlan.BoundaryModel);
	return true;
}

bool FBlueprintHelperGraphBodyReplaceCoordinator::CanAcceptAdapterPlanConnectivityDiagnostics(
	const FBlueprintHelperGraphBodyReplacePlan& ReplacePlan,
	const FBlueprintGenerateResult& GenerateResult,
	const TSet<UEdGraphNode*>& NodesBeforeImport)
{
	if (!ReplacePlan.ReconnectPlan.bReconnectEntryToFirstImportedExec)
	{
		return false;
	}
	if (ReplacePlan.ReconnectPlan.bReconnectImportedExecToExitBoundary &&
		!ReplacePlan.ConnectivityPolicy.bAllowExitBoundaryReachability)
	{
		return false;
	}
	if (GenerateResult.bSucceed ||
		GenerateResult.UnresolvedNodeCount != 0 ||
		GenerateResult.ConnectivityDiagnostics.Num() == 0)
	{
		return false;
	}

	for (const FBlueprintGeneratorDiagnostic& Diagnostic : GenerateResult.ConnectivityDiagnostics)
	{
		if (Diagnostic.Code != TEXT("unreachable_exec_node") &&
			Diagnostic.Code != TEXT("unreachable_pure_data_chain") &&
			Diagnostic.Code != TEXT("missing_expected_link") &&
			Diagnostic.Code != TEXT("unconsumed_pure_data_node"))
		{
			return false;
		}
	}

	UEdGraph* Graph = ReplacePlan.Target.Graph;
	const TArray<UEdGraphNode*> ImportedNodes =
		FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::CollectImportedNodes(Graph, NodesBeforeImport);
	TSet<UEdGraphNode*> ImportedNodeSet;
	for (UEdGraphNode* Node : ImportedNodes)
	{
		if (Node)
		{
			ImportedNodeSet.Add(Node);
		}
	}
	if (ImportedNodeSet.Num() == 0)
	{
		return false;
	}

	TSet<UEdGraphNode*> BodyFlowNodeSet = ImportedNodeSet;
	FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::AddReachedExitBoundaries(
		ReplacePlan.Target.ExitBoundaryNodes,
		BodyFlowNodeSet);

	UEdGraphNode* BodyEntryNode =
		FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::FindFirstImportedExecutableBodyNode(Graph, NodesBeforeImport);
	UEdGraphPin* BodyEntryPin =
		FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::FindFirstExecPin(BodyEntryNode, EGPD_Input);
	if (!BodyEntryPin || !BodyFlowNodeSet.Contains(BodyEntryNode))
	{
		return false;
	}

	TSet<UEdGraphNode*> ReachableExecNodes;
	FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::CollectExecReachableFromBodyEntry(
		BodyEntryPin,
		BodyFlowNodeSet,
		ReachableExecNodes);
	if (ReachableExecNodes.Num() == 0)
	{
		return false;
	}

	return FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::ImportedExecNodesReachBodyEntryExecFlow(
		ImportedNodes,
		ReachableExecNodes) &&
		FBlueprintHelperGraphBodyReplaceCoordinatorLocalUtils::GeneratedPureDataChainsReachBodyEntryExecFlow(
			ImportedNodes,
			BodyFlowNodeSet,
			ReachableExecNodes);
}
