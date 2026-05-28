#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"

#include "EdGraph/EdGraphPin.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "UObject/UnrealType.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

FBlueprintHelperEventDelegateBindingObjectResolution FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	const FBlueprintHelperNodeFragment& Fragment)
{
	const FString BindingKind = Evidence.BindingObjectKind.TrimStartAndEnd().ToLower();
	if (BindingKind.IsEmpty())
	{
		return UGraphWriteGraphStatementUtils::Fail(TEXT("missing_binding_object_evidence"));
	}

	FBlueprintHelperEventDelegateBindingObjectResolution Resolution;
	Resolution.bResolved = true;
	Resolution.ObjectEvidenceId = Evidence.BindingObjectEvidenceId;

	if (BindingKind == TEXT("self"))
	{
		return Resolution;
	}

	Resolution.ObjectPin = UGraphWriteGraphStatementUtils::FindProjectedPin(Fragment, Evidence.BindingObjectEvidenceId);
	if (!Resolution.ObjectPin)
	{
		Resolution.ObjectPin = UGraphWriteGraphStatementUtils::FindProjectedPin(Fragment, FString::Printf(TEXT("event_delegate.binding_object.%s"), *Evidence.BindingObjectEvidenceId));
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
			return UGraphWriteGraphStatementUtils::Fail(TEXT("binding_object_linked_pin_anchor_missing"));
		}
		if (BindingKind == TEXT("function_return_ref")
			&& !Evidence.BindingObjectProducerStatementId.IsEmpty()
			&& !Evidence.BindingObjectProducerStatementId.Equals(Fragment.SourceStatementId, ESearchCase::IgnoreCase))
		{
			return UGraphWriteGraphStatementUtils::Fail(TEXT("binding_object_cross_statement_unsupported"));
		}
		if (!Resolution.ObjectPin)
		{
			return UGraphWriteGraphStatementUtils::Fail(FString::Printf(TEXT("binding_object_%s_pin_missing"), *BindingKind));
		}
		return Resolution;
	}

	return UGraphWriteGraphStatementUtils::Fail(TEXT("unsupported_binding_object_kind"));
}
