#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"

#include "EdGraph/EdGraphPin.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "UObject/UnrealType.h"

namespace
{
static FBlueprintHelperEventDelegateBindingObjectResolution Fail(const FString& Code)
{
	FBlueprintHelperEventDelegateBindingObjectResolution Resolution;
	Resolution.bResolved = false;
	Resolution.ErrorCode = Code;
	return Resolution;
}

static UEdGraphPin* FindProjectedPin(
	const FBlueprintHelperNodeFragment& Fragment,
	const FString& EvidenceId)
{
	if (EvidenceId.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	if (const FBlueprintHelperFragmentPinRef* PinRef = Fragment.PinBindings.Find(EvidenceId))
	{
		return PinRef->Pin;
	}
	if (const FBlueprintHelperFragmentPinRef* PinRef = Fragment.DataOutputs.Find(EvidenceId))
	{
		return PinRef->Pin;
	}
	return nullptr;
}
}

FBlueprintHelperEventDelegateBindingObjectResolution FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	const FBlueprintHelperNodeFragment& Fragment)
{
	const FString BindingKind = Evidence.BindingObjectKind.TrimStartAndEnd().ToLower();
	if (BindingKind.IsEmpty())
	{
		return Fail(TEXT("missing_binding_object_evidence"));
	}

	FBlueprintHelperEventDelegateBindingObjectResolution Resolution;
	Resolution.bResolved = true;
	Resolution.ObjectEvidenceId = Evidence.BindingObjectEvidenceId;

	if (BindingKind == TEXT("self"))
	{
		return Resolution;
	}

	Resolution.ObjectPin = FindProjectedPin(Fragment, Evidence.BindingObjectEvidenceId);
	if (!Resolution.ObjectPin)
	{
		Resolution.ObjectPin = FindProjectedPin(Fragment, FString::Printf(TEXT("event_delegate.binding_object.%s"), *Evidence.BindingObjectEvidenceId));
	}

	if (BindingKind == TEXT("component_ref"))
	{
		return Resolution;
	}

	if (BindingKind == TEXT("field_get_ref")
		|| BindingKind == TEXT("linked_pin_ref")
		|| BindingKind == TEXT("function_return_ref"))
	{
		if (BindingKind == TEXT("linked_pin_ref")
			&& (Evidence.BindingObjectNodeGuid.IsEmpty() || Evidence.BindingObjectPinName.IsEmpty()))
		{
			return Fail(TEXT("binding_object_linked_pin_anchor_missing"));
		}
		if (BindingKind == TEXT("function_return_ref")
			&& !Evidence.BindingObjectProducerStatementId.IsEmpty()
			&& !Evidence.BindingObjectProducerStatementId.Equals(Fragment.SourceStatementId, ESearchCase::IgnoreCase))
		{
			return Fail(TEXT("binding_object_cross_statement_unsupported"));
		}
		if (!Resolution.ObjectPin)
		{
			return Fail(FString::Printf(TEXT("binding_object_%s_pin_missing"), *BindingKind));
		}
		return Resolution;
	}

	return Fail(TEXT("unsupported_binding_object_kind"));
}
