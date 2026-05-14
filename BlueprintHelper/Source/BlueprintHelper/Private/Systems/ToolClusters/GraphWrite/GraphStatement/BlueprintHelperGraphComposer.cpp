#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"

namespace
{
static UEdGraphPin* FindPinRefInMap(
	const TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
	const FString& Key)
{
	if (Key.IsEmpty())
	{
		return nullptr;
	}

	if (const FBlueprintHelperFragmentPinRef* PinRef = PinMap.Find(Key))
	{
		if (PinRef->Pin)
		{
			return PinRef->Pin;
		}
	}

	for (const TPair<FString, FBlueprintHelperFragmentPinRef>& Pair : PinMap)
	{
		if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase) && Pair.Value.Pin)
		{
			return Pair.Value.Pin;
		}
	}

	return nullptr;
}

static UEdGraphPin* FindNodePinByName(UEdGraphNode* Node, const FString& PinName)
{
	if (!Node || PinName.IsEmpty())
	{
		return nullptr;
	}

	if (UEdGraphPin* ExactPin = Node->FindPin(PinName))
	{
		return ExactPin;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}

	return nullptr;
}

static UEdGraphPin* ResolveFragmentEndpointPin(
	const FBlueprintHelperNodeFragment& Fragment,
	const FBlueprintHelperGraphFragmentEndpointRef& Endpoint,
	const bool bSourceEndpoint)
{
	const FString PortId = Endpoint.PortId.IsEmpty() ? Endpoint.PinName : Endpoint.PortId;
	if (!PortId.IsEmpty())
	{
		if (UEdGraphPin* BindingPin = FindPinRefInMap(Fragment.PinBindings, PortId))
		{
			return BindingPin;
		}

		const TMap<FString, FBlueprintHelperFragmentPinRef>& DirectionMap =
			bSourceEndpoint ? Fragment.DataOutputs : Fragment.DataInputs;
		if (UEdGraphPin* DirectionPin = FindPinRefInMap(DirectionMap, PortId))
		{
			return DirectionPin;
		}
	}

	if (!Endpoint.PinName.IsEmpty())
	{
		const TMap<FString, FBlueprintHelperFragmentPinRef>& DirectionMap =
			bSourceEndpoint ? Fragment.DataOutputs : Fragment.DataInputs;
		if (UEdGraphPin* DirectionPin = FindPinRefInMap(DirectionMap, Endpoint.PinName))
		{
			return DirectionPin;
		}

		if (UEdGraphPin* BindingPin = FindPinRefInMap(Fragment.PinBindings, Endpoint.PinName))
		{
			return BindingPin;
		}

		if (UEdGraphPin* NodePin = FindNodePinByName(Fragment.PrimaryNode, Endpoint.PinName))
		{
			return NodePin;
		}
	}

	return nullptr;
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

		UEdGraphPin* FromPin = ResolveFragmentEndpointPin(**FromFragmentPtr, Edge.From, true);
		UEdGraphPin* ToPin = ResolveFragmentEndpointPin(**ToFragmentPtr, Edge.To, false);
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

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++Result.CreatedDataConnectionCount;
			continue;
		}

		Result.Diagnostics.Add(ConnectionResponse.Message.IsEmpty()
			? FString::Printf(
				TEXT("GraphComposer rejected data edge: %s.%s -> %s.%s."),
				*Edge.From.FragmentId,
				*Edge.From.PortId,
				*Edge.To.FragmentId,
				*Edge.To.PortId)
			: ConnectionResponse.Message.ToString());
	}

	Result.bSucceeded = Result.Diagnostics.Num() == 0;
	return Result;
}
