#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphComposerUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"

namespace
{
static bool NodeHasWildcardPins(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			return true;
		}
	}
	return false;
}

static void AddNodeForDeferredReconstruct(UEdGraphPin* Pin, TSet<UEdGraphNode*>& NodesToReconstruct)
{
	UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
	if (Node && NodeHasWildcardPins(Node))
	{
		NodesToReconstruct.Add(Node);
	}
}

static void ReconstructWildcardNodes(const TSet<UEdGraphNode*>& NodesToReconstruct)
{
	for (UEdGraphNode* Node : NodesToReconstruct)
	{
		if (UK2Node* K2Node = Cast<UK2Node>(Node))
		{
			K2Node->ReconstructNode();
		}
		Node->NodeConnectionListChanged();
	}
}
}

FBlueprintHelperGraphComposeResult FBlueprintHelperGraphComposer::ConnectLinearExecChain(
	UEdGraph* TargetGraph,
	const TArray<FBlueprintHelperNodeFragment>& Fragments)
{
	FBlueprintHelperGraphComposeResult Result;

	if (!TargetGraph)
	{
		Result.Diagnostics.Add(TEXT("GraphComposer failed: target graph is invalid."));
		return Result;
	}

	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (!Schema)
	{
		Result.Diagnostics.Add(TEXT("GraphComposer failed: graph schema is invalid."));
		return Result;
	}

	const FBlueprintHelperNodeFragment* PreviousExecutable = nullptr;
	for (const FBlueprintHelperNodeFragment& Fragment : Fragments)
	{
		if (!Fragment.IsValid() || !Fragment.ExecEntryPin || !Fragment.ExecExitPin)
		{
			continue;
		}

		if (!PreviousExecutable)
		{
			PreviousExecutable = &Fragment;
			continue;
		}

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(
			PreviousExecutable->ExecExitPin,
			Fragment.ExecEntryPin);
		if (Schema->TryCreateConnection(PreviousExecutable->ExecExitPin, Fragment.ExecEntryPin))
		{
			++Result.CreatedExecConnectionCount;
			PreviousExecutable = &Fragment;
			continue;
		}

		Result.Diagnostics.Add(ConnectionResponse.Message.IsEmpty()
			? FString::Printf(
				TEXT("GraphComposer rejected exec connection: %s -> %s."),
				*PreviousExecutable->SourceStatementId,
				*Fragment.SourceStatementId)
			: ConnectionResponse.Message.ToString());
		PreviousExecutable = &Fragment;
	}

	Result.bSucceeded = Result.Diagnostics.Num() == 0;
	return Result;
}

FBlueprintHelperGraphComposeResult FBlueprintHelperGraphComposer::ConnectDataEdges(
	UEdGraph* TargetGraph,
	const TArray<FBlueprintHelperNodeFragment>& Fragments,
	const TArray<FBlueprintHelperGraphFragmentDataEdge>& DataEdges)
{
	FBlueprintHelperGraphComposeResult Result;
	if (!TargetGraph)
	{
		Result.Diagnostics.Add(TEXT("GraphComposer data edge connection failed: target graph is invalid."));
		return Result;
	}

	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (!Schema)
	{
		Result.Diagnostics.Add(TEXT("GraphComposer data edge connection failed: graph schema is invalid."));
		return Result;
	}

	TMap<FString, const FBlueprintHelperNodeFragment*> FragmentById;
	for (const FBlueprintHelperNodeFragment& Fragment : Fragments)
	{
		if (!Fragment.FragmentId.IsEmpty())
		{
			FragmentById.Add(Fragment.FragmentId, &Fragment);
		}
		}

	TSet<UEdGraphNode*> NodesToReconstruct;
	for (const FBlueprintHelperGraphFragmentDataEdge& Edge : DataEdges)
	{
		const FBlueprintHelperNodeFragment* const* FromFragmentPtr = FragmentById.Find(Edge.From.FragmentId);
		const FBlueprintHelperNodeFragment* const* ToFragmentPtr = FragmentById.Find(Edge.To.FragmentId);
		if (!FromFragmentPtr || !*FromFragmentPtr)
		{
			Result.Diagnostics.Add(FString::Printf(TEXT("GraphComposer data edge source fragment not found: %s."), *Edge.From.FragmentId));
			continue;
		}
		if (!ToFragmentPtr || !*ToFragmentPtr)
		{
			Result.Diagnostics.Add(FString::Printf(TEXT("GraphComposer data edge target fragment not found: %s."), *Edge.To.FragmentId));
			continue;
		}

		UEdGraphPin* FromPin = FBlueprintHelperGraphComposerUtils::ResolveFragmentEndpointPin(**FromFragmentPtr, Edge.From, true);
		UEdGraphPin* ToPin = FBlueprintHelperGraphComposerUtils::ResolveFragmentEndpointPin(**ToFragmentPtr, Edge.To, false);
		if (!FromPin)
		{
			Result.Diagnostics.Add(FString::Printf(TEXT("GraphComposer data edge source pin not found: %s.%s."), *Edge.From.FragmentId, *Edge.From.PortId));
			continue;
		}
		if (!ToPin)
		{
			Result.Diagnostics.Add(FString::Printf(TEXT("GraphComposer data edge target pin not found: %s.%s."), *Edge.To.FragmentId, *Edge.To.PortId));
			continue;
		}
		if (FromPin->LinkedTo.Contains(ToPin))
		{
			continue;
		}

		AddNodeForDeferredReconstruct(FromPin, NodesToReconstruct);
		AddNodeForDeferredReconstruct(ToPin, NodesToReconstruct);
		FString ConnectionFailureReason;
		if (FBlueprintHelperGraphComposerUtils::TryCreateSchemaDataConnection(FromPin, ToPin, ConnectionFailureReason))
		{
			++Result.CreatedDataConnectionCount;
			continue;
		}

		Result.Diagnostics.Add(ConnectionFailureReason.IsEmpty()
			? FString::Printf(
				TEXT("GraphComposer rejected data edge: %s.%s -> %s.%s."),
				*Edge.From.FragmentId,
				*Edge.From.PortId,
				*Edge.To.FragmentId,
				*Edge.To.PortId)
			: ConnectionFailureReason);
	}

	ReconstructWildcardNodes(NodesToReconstruct);

	Result.bSucceeded = Result.Diagnostics.Num() == 0;
	return Result;
}
