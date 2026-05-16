#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphComposerUtils.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

UEdGraphPin* FBlueprintHelperGraphComposerUtils::FindPinRefInMap(
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

UEdGraphPin* FBlueprintHelperGraphComposerUtils::FindNodePinByName(
	UEdGraphNode* Node,
	const FString& PinName)
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

UEdGraphPin* FBlueprintHelperGraphComposerUtils::FindFirstNodeDataPin(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory != FName(TEXT("exec")))
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphPin* FBlueprintHelperGraphComposerUtils::ResolveFragmentEndpointPin(
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

	if (Endpoint.PinName.Equals(TEXT("result"), ESearchCase::IgnoreCase)
		|| Endpoint.PinName.Equals(TEXT("value"), ESearchCase::IgnoreCase)
		|| Endpoint.PinName.Equals(TEXT("return"), ESearchCase::IgnoreCase))
	{
		return FindFirstNodeDataPin(Fragment.PrimaryNode, bSourceEndpoint ? EGPD_Output : EGPD_Input);
	}

	return nullptr;
}

bool FBlueprintHelperGraphComposerUtils::TryForceCompatibleDataConnection(
	UEdGraphPin* FromPin,
	UEdGraphPin* ToPin)
{
	if (!FromPin || !ToPin || FromPin->LinkedTo.Contains(ToPin))
	{
		return false;
	}

	if (FromPin->PinType.PinCategory == FName(TEXT("exec")) || ToPin->PinType.PinCategory == FName(TEXT("exec")))
	{
		return false;
	}

	if (FromPin->Direction != EGPD_Output || ToPin->Direction != EGPD_Input)
	{
		return false;
	}

	if (FromPin->PinType.PinCategory != ToPin->PinType.PinCategory)
	{
		return false;
	}

	FromPin->MakeLinkTo(ToPin);
	if (UEdGraphNode* FromNode = FromPin->GetOwningNode())
	{
		FromNode->NodeConnectionListChanged();
	}
	if (UEdGraphNode* ToNode = ToPin->GetOwningNode())
	{
		ToNode->NodeConnectionListChanged();
	}
	return true;
}
