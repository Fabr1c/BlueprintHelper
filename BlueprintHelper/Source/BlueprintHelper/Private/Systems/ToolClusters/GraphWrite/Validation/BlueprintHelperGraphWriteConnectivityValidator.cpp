#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "Shared/BlueprintHelperVersionCompat.h"

namespace BlueprintHelperGraphWriteConnectivityValidation
{
	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
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

	static bool HasIncomingExecLink(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && IsExecPin(Pin) && Pin->LinkedTo.Num() > 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasOutgoingDataConsumer(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->Direction == EGPD_Input && !IsExecPin(LinkedPin))
				{
					return true;
				}
			}
		}
		return false;
	}

	static TSet<const UEdGraphNode*> ResolveBoundaryNodes(
		const TArray<FString>& BoundaryRefs,
		const TMap<FString, UEdGraphNode*>& NodeRefs)
	{
		TSet<const UEdGraphNode*> Nodes;
		for (const FString& BoundaryRef : BoundaryRefs)
		{
			UEdGraphNode* const* Node = NodeRefs.Find(BoundaryRef);
			if (Node && *Node)
			{
				Nodes.Add(*Node);
			}
		}
		return Nodes;
	}

	static bool BoundaryRefsContainNode(
		const TArray<FString>& BoundaryRefs,
		const TMap<FString, UEdGraphNode*>& NodeRefs,
		const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (const FString& BoundaryRef : BoundaryRefs)
		{
			UEdGraphNode* const* BoundaryNode = NodeRefs.Find(BoundaryRef);
			if (BoundaryNode && *BoundaryNode == Node)
			{
				return true;
			}
		}
		return false;
	}

	static TSet<const UEdGraphNode*> CollectLegalTerminalNodes(
		const FBlueprintGraphWriteConnectivityValidationInput& Input)
	{
		TSet<const UEdGraphNode*> Nodes = ResolveBoundaryNodes(Input.BoundaryModel.ExitNodeRefs, Input.NodeRefs);
		for (UEdGraphNode* Node : Input.AllowedTerminalPureDataNodes)
		{
			if (Node)
			{
				Nodes.Add(Node);
			}
		}
		return Nodes;
	}

	static bool IsPolicyAllowedViolation(
		const FBlueprintGraphWriteConnectivityValidationInput& Input,
		const FString& Code)
	{
		return Input.ConnectivityPolicy.ViolationCodes.Contains(Code)
			|| Input.BoundaryModel.ConnectivityExceptionCodes.Contains(Code);
	}

	static TSet<const UEdGraphNode*> CollectReachableExecNodes(
		const TSet<const UEdGraphNode*>& EntryNodes)
	{
		TSet<const UEdGraphNode*> ReachableNodes;
		TArray<const UEdGraphNode*> PendingNodes;

		for (const UEdGraphNode* EntryRoot : EntryNodes)
		{
			if (EntryRoot)
			{
				ReachableNodes.Add(EntryRoot);
				PendingNodes.Add(EntryRoot);
			}
		}

		while (PendingNodes.Num() > 0)
		{
			const UEdGraphNode* Node = FBlueprintHelperVersionCompat::PopNoShrink(PendingNodes);
			if (!Node)
			{
				continue;
			}

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
				{
					continue;
				}

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && HasExecPin(LinkedNode) && !ReachableNodes.Contains(LinkedNode))
					{
						ReachableNodes.Add(LinkedNode);
						PendingNodes.Add(LinkedNode);
					}
				}
			}
		}

		return ReachableNodes;
	}

	static bool DataChainReachesExecConsumer(
		const UEdGraphNode* Node,
		const TSet<const UEdGraphNode*>& ReachableExecNodes,
		const TSet<const UEdGraphNode*>& LegalTerminalNodes,
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

				const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
				if (!LinkedNode)
				{
					continue;
				}

				if (LegalTerminalNodes.Contains(LinkedNode))
				{
					return true;
				}

				if (HasExecPin(LinkedNode))
				{
					if (ReachableExecNodes.Contains(LinkedNode))
					{
						return true;
					}
					continue;
				}

				if (DataChainReachesExecConsumer(LinkedNode, ReachableExecNodes, LegalTerminalNodes, VisitedNodes))
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool IsConnectivityWhitelisted(const UEdGraphNode* Node)
	{
		return Node
			&& (Node->IsA<UEdGraphNode_Comment>() || Node->IsA<UK2Node_Knot>());
	}

	static bool IsAllowedIsolatedNode(
		const FBlueprintGraphWriteConnectivityValidationInput& Input,
		const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		if (Input.BoundaryModel.AllowedIsolatedNodePolicy
			== EBlueprintHelperGraphBodyIsolatedNodePolicy::CommentsAndReroutesOnly)
		{
			return IsConnectivityWhitelisted(Node);
		}

		return BoundaryRefsContainNode(Input.BoundaryModel.EntryNodeRefs, Input.NodeRefs, Node)
			|| BoundaryRefsContainNode(Input.BoundaryModel.ExitNodeRefs, Input.NodeRefs, Node)
			|| BoundaryRefsContainNode(Input.BoundaryModel.ProtectedNodeRefs, Input.NodeRefs, Node)
			|| BoundaryRefsContainNode(Input.BoundaryModel.ExternalAnchorRefs, Input.NodeRefs, Node);
	}

	static FString NodeDiagnosticId(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("generated_node");
		}
		if (!Node->NodeGuid.IsValid())
		{
			return Node->GetName();
		}
		return Node->NodeGuid.ToString(EGuidFormats::Digits);
	}

	static FString NodeDiagnosticLabel(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("generated node");
		}

		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimStartAndEnd();
		return Title.IsEmpty() ? Node->GetClass()->GetName() : Title;
	}

	static FBlueprintGeneratorDiagnostic MakeDiagnostic(
		const FString& Code,
		const UEdGraphNode* Node,
		const FString& Message)
	{
		FBlueprintGeneratorDiagnostic Diagnostic;
		Diagnostic.Severity = TEXT("error");
		Diagnostic.Code = Code;
		Diagnostic.NodeId = NodeDiagnosticId(Node);
		Diagnostic.Message = FString::Printf(TEXT("%s: %s"), *NodeDiagnosticLabel(Node), *Message);
		return Diagnostic;
	}
}

FBlueprintGraphWriteConnectivityValidationResult FBlueprintHelperGraphWriteConnectivityValidator::Validate(
	const FBlueprintGraphWriteConnectivityValidationInput& Input)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidation;

	FBlueprintGraphWriteConnectivityValidationResult Result;
	const TSet<const UEdGraphNode*> EntryNodes =
		ResolveBoundaryNodes(Input.BoundaryModel.EntryNodeRefs, Input.NodeRefs);
	const TSet<const UEdGraphNode*> LegalTerminalNodes = CollectLegalTerminalNodes(Input);
	const TSet<const UEdGraphNode*> ReachableExecNodes = CollectReachableExecNodes(EntryNodes);

	if (Input.RequestedConnectionCount > Input.CreatedConnectionCount)
	{
		Result.Diagnostics.Add(MakeDiagnostic(
			TEXT("missing_expected_link"),
			nullptr,
			FString::Printf(
				TEXT("GraphWrite requested %d links but only created %d."),
				Input.RequestedConnectionCount,
				Input.CreatedConnectionCount)));
	}

	for (UEdGraphNode* Node : Input.GeneratedNodes)
	{
		if (!Node || IsAllowedIsolatedNode(Input, Node))
		{
			continue;
		}

		if (HasExecPin(Node))
		{
			if (EntryNodes.Contains(Node) || LegalTerminalNodes.Contains(Node))
			{
				continue;
			}
			if (!ReachableExecNodes.Contains(Node)
				&& !IsPolicyAllowedViolation(Input, TEXT("unreachable_exec_node")))
			{
				Result.Diagnostics.Add(MakeDiagnostic(
					TEXT("unreachable_exec_node"),
					Node,
					HasIncomingExecLink(Node)
						? TEXT("Generated exec node has incoming exec links but is not reachable from an entry root.")
						: TEXT("Generated exec node has no incoming exec link.")));
			}
			continue;
		}

		if (Input.BoundaryModel.PureDataConsumptionPolicy
			== EBlueprintHelperGraphBodyPureDataPolicy::PureDataGraphOutput)
		{
			continue;
		}

		if (!HasOutgoingDataConsumer(Node))
		{
			if (!LegalTerminalNodes.Contains(Node)
				&& Input.BoundaryModel.PureDataConsumptionPolicy
					!= EBlueprintHelperGraphBodyPureDataPolicy::AllowTerminalPureDataOutput
				&& !IsPolicyAllowedViolation(Input, TEXT("unconsumed_pure_data_node")))
			{
				Result.Diagnostics.Add(MakeDiagnostic(
					TEXT("unconsumed_pure_data_node"),
					Node,
					TEXT("Generated PureData node has no outgoing data consumer.")));
			}
		}
		else if (Input.BoundaryModel.PureDataConsumptionPolicy
			== EBlueprintHelperGraphBodyPureDataPolicy::RequireReachableExecConsumer)
		{
			TSet<const UEdGraphNode*> VisitedNodes;
			if (!DataChainReachesExecConsumer(Node, ReachableExecNodes, LegalTerminalNodes, VisitedNodes)
				&& !IsPolicyAllowedViolation(Input, TEXT("unreachable_pure_data_chain")))
			{
				Result.Diagnostics.Add(MakeDiagnostic(
					TEXT("unreachable_pure_data_chain"),
					Node,
					TEXT("Generated PureData node output chain does not reach a reachable exec node input.")));
			}
		}
	}

	Result.bPassed = Result.Diagnostics.Num() == 0;
	return Result;
}
