#include "Systems/ToolClusters/GraphWrite/Readback/Utils/GraphWriteReadbackUtils.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CreateDelegate.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

void UGraphWriteReadbackUtils::AddIfPresent(TMap<FString, FString>& Facts, const FString& Key, const FString& Value)
{
	if (!Key.IsEmpty() && !Value.TrimStartAndEnd().IsEmpty())
	{
		Facts.Add(Key, Value.TrimStartAndEnd());
	}
}

void UGraphWriteReadbackUtils::AddPrimaryNodeFacts(
	const FBlueprintHelperNodeFragment& Fragment,
	TMap<FString, FString>& Facts)
{
	UEdGraphNode* PrimaryNode = Fragment.PrimaryNode;
	if (!PrimaryNode)
	{
		return;
	}

	Facts.Add(TEXT("node_class"), PrimaryNode->GetClass()->GetPathName());
	Facts.Add(TEXT("node_guid"), PrimaryNode->NodeGuid.ToString(EGuidFormats::Digits));
	if (const UK2Node_ComponentBoundEvent* ComponentEvent = Cast<UK2Node_ComponentBoundEvent>(PrimaryNode))
	{
		Facts.Add(TEXT("component_dynamic_binding_target"), ComponentEvent->GetComponentPropertyName().ToString());
		Facts.Add(TEXT("delegate_property_name"), ComponentEvent->DelegatePropertyName.ToString());
	}
}

void UGraphWriteReadbackUtils::AddPinFacts(
	const FBlueprintHelperNodeFragment& Fragment,
	TMap<FString, FString>& Facts)
{
	for (const TPair<FString, FBlueprintHelperFragmentPinRef>& PinPair : Fragment.PinBindings)
	{
		if (!PinPair.Value.Pin)
		{
			continue;
		}
		const FString Prefix = FString::Printf(TEXT("pin.%s."), *PinPair.Key);
		Facts.FindOrAdd(Prefix + TEXT("name")) = PinPair.Value.Pin->PinName.ToString();
		Facts.FindOrAdd(Prefix + TEXT("type")) = PinPair.Value.Pin->PinType.PinCategory.ToString();
		if (!PinPair.Value.Pin->DefaultValue.IsEmpty())
		{
			Facts.FindOrAdd(Prefix + TEXT("default")) = PinPair.Value.Pin->DefaultValue;
		}
		if (PinPair.Value.Pin->LinkedTo.Num() > 0 && PinPair.Value.Pin->LinkedTo[0])
		{
			Facts.FindOrAdd(Prefix + TEXT("linked_source_pin")) = PinPair.Value.Pin->LinkedTo[0]->PinName.ToString();
		}
	}
}
