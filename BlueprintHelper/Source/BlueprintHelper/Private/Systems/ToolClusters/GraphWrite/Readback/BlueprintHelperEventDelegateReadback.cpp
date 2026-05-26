#include "Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CreateDelegate.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

namespace
{
static void AddIfPresent(TMap<FString, FString>& Facts, const FString& Key, const FString& Value)
{
	if (!Key.IsEmpty() && !Value.TrimStartAndEnd().IsEmpty())
	{
		Facts.Add(Key, Value.TrimStartAndEnd());
	}
}

static void AddPrimaryNodeFacts(
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

static void AddPinFacts(
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
}

FBlueprintHelperEventDelegateReadbackFacts FBlueprintHelperEventDelegateReadback::Collect(
	const FBlueprintHelperNodeFragment& Fragment,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	FBlueprintHelperEventDelegateReadbackFacts Result;
	TMap<FString, FString>& Facts = Result.Facts;

	AddPrimaryNodeFacts(Fragment, Facts);
	AddIfPresent(Facts, TEXT("spawner_or_factory_kind"), Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
		? TEXT("ue_delegate_manual_assign_factory")
		: TEXT("ue_delegate_node_spawner"));
	AddIfPresent(Facts, TEXT("delegate_owner_class_path"), Evidence.DelegateOwnerClassPath);
	AddIfPresent(Facts, TEXT("delegate_property_path"), Evidence.DelegatePropertyPath);
	AddIfPresent(Facts, TEXT("delegate_signature_function_path"), Evidence.DelegateSignatureFunctionPath);
	AddIfPresent(Facts, TEXT("binding_object_kind"), Evidence.BindingObjectKind);
	AddIfPresent(Facts, TEXT("binding_object_evidence_id"), Evidence.BindingObjectEvidenceId);
	AddIfPresent(Facts, TEXT("handler_function_path"), Evidence.HandlerFunctionPath);
	AddIfPresent(Facts, TEXT("statement_id"), Fragment.SourceStatementId);
	if (!Facts.FindRef(TEXT("statement_id")).IsEmpty() && !Facts.FindRef(TEXT("node_guid")).IsEmpty())
	{
		Facts.Add(
			TEXT("compile_diagnostic_correlation_key"),
			FString::Printf(TEXT("%s:%s"), *Facts.FindRef(TEXT("statement_id")), *Facts.FindRef(TEXT("node_guid"))));
	}

	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		if (const UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Node))
		{
			AddIfPresent(Facts, TEXT("create_delegate.handler_name"), CreateDelegate->GetFunctionName().ToString());
		}
	}

	AddPinFacts(Fragment, Facts);
	return Result;
}
