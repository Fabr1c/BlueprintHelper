#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

bool FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
	const FBlueprintHelperActionResolutionRequest& Request,
	EBlueprintHelperActionSemanticKind SemanticKind,
	FBlueprintHelperEventDelegateUseSiteEvidence& OutEvidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperEventDelegateUseSiteEvidence();
	OutMissingDetail.Reset();
	OutMessage.Reset();

	OutEvidence.SemanticKind = SemanticKind;
	OutEvidence.DelegateOperation = UGraphWriteActionEvidenceUtils::NormalizeDelegateOperationToken(UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("operation")));
	OutEvidence.DelegateName = UGraphWriteActionEvidenceUtils::FirstNonEmpty({
		UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_name")),
		Request.Semantic.PropertyPath,
		Request.Semantic.Query
	});
	OutEvidence.DelegateOwnerClassPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_owner_class_path"));
	OutEvidence.DelegatePropertyName = UGraphWriteActionEvidenceUtils::FirstNonEmpty({
		UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_property_name")),
		OutEvidence.DelegateName
	});
	OutEvidence.DelegatePropertyPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_property_path"));
	OutEvidence.DelegateSignature = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_signature"));
	OutEvidence.DelegateSignatureFunctionPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("delegate_signature_function_path"));
	OutEvidence.ComponentPath = UGraphWriteActionEvidenceUtils::FirstNonEmpty({
		UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("component_property_name")),
		UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("component_path")),
		SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
			? UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_path"))
			: FString()
	});
	OutEvidence.ComponentBindingOwnerClassPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("component_binding_owner_class_path"));
	OutEvidence.ComponentBindingFieldPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("component_binding_field_path"));
	OutEvidence.ComponentClassPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("component_class_path"));
	OutEvidence.BindingObjectKind = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_kind"));
	OutEvidence.BindingObjectEvidenceId = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_evidence_id"));
	OutEvidence.BindingObjectPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_path"));
	OutEvidence.BindingObjectProducerStatementId = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_statement_id"));
	OutEvidence.BindingObjectNodeGuid = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_node_guid"));
	OutEvidence.BindingObjectPinName = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_pin_name"));
	OutEvidence.BindingObjectError = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("binding_object_error"));
	OutEvidence.HandlerName = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("handler_name"));
	OutEvidence.HandlerScopeClassPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("handler_scope_class_path"));
	OutEvidence.HandlerFunctionPath = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("handler_function_path"));
	OutEvidence.HandlerSourceCluster = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("handler_source_cluster"));
	OutEvidence.SignatureEvidenceId = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("signature_evidence_id"));
	OutEvidence.UnbindMode = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("unbind_mode"));
	OutEvidence.DuplicatePolicy = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("duplicate_policy"));
	OutEvidence.AssignFactory = UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(Request, TEXT("assign_factory"));

	if (SemanticKind != EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& SemanticKind != EBlueprintHelperActionSemanticKind::Delegate)
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(
			TEXT("invalid_delegate_operation"),
			FString::Printf(TEXT("EventDelegate use-site evidence does not support semantic '%s'."), *FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind)),
			OutMissingDetail,
			OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate)
	{
		if (!OutEvidence.BindingObjectError.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(
				OutEvidence.BindingObjectError,
				FString::Printf(TEXT("EventDelegate binding object projection failed: %s."), *OutEvidence.BindingObjectError),
				OutMissingDetail,
				OutMessage);
		}
		if (OutEvidence.DelegateOperation.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("invalid_delegate_operation"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.operation."), OutMissingDetail, OutMessage);
		}
		if (!UGraphWriteActionEvidenceUtils::IsSupportedDelegateOperation(OutEvidence.DelegateOperation))
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(
				TEXT("invalid_delegate_operation"),
				FString::Printf(TEXT("Unsupported delegate_operation '%s'."), *OutEvidence.DelegateOperation),
				OutMissingDetail,
				OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& OutEvidence.ComponentPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_property_name."), OutMissingDetail, OutMessage);
	}
	if (UGraphWriteActionEvidenceUtils::RequiresBindingObject(SemanticKind)
		&& OutEvidence.BindingObjectKind.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.binding_object_kind."), OutMissingDetail, OutMessage);
	}
	if (UGraphWriteActionEvidenceUtils::RequiresBindingObject(SemanticKind)
		&& OutEvidence.BindingObjectEvidenceId.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.binding_object_evidence_id."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateName.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_name or semantic delegate query."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateOwnerClassPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_owner_class_path."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyName.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_name."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_path."), OutMissingDetail, OutMessage);
	}
	if (!OutEvidence.DelegateOperation.Equals(TEXT("clear"), ESearchCase::IgnoreCase)
		&& OutEvidence.DelegateSignatureFunctionPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_signature_evidence"), TEXT("EventDelegate use-site resolution requires ContextEvidence.event_delegate.delegate_signature_function_path."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.ComponentBindingOwnerClassPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_binding_owner_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.ComponentBindingFieldPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_binding_field_path."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& UGraphWriteActionEvidenceUtils::RequiresHandlerForOperation(OutEvidence.DelegateOperation))
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(
				OutEvidence.DelegateOperation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase) ? TEXT("handler_required_for_unbind") : TEXT("missing_handler_evidence"),
				TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.event_delegate.handler_name."),
				OutMissingDetail,
				OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.event_delegate.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerFunctionPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_function_path from BlueprintSignature."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerSourceCluster.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_source_cluster."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_signature_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.signature_evidence_id."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.handler_name."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerFunctionPath.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_function_path from BlueprintSignature."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerSourceCluster.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_source_cluster."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_signature_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.signature_evidence_id."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase)
		&& !OutEvidence.UnbindMode.Equals(TEXT("single"), ESearchCase::IgnoreCase))
	{
		return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("handler_required_for_unbind"), TEXT("delegate unbind requires ContextEvidence.event_delegate.unbind_mode=single."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		if (!OutEvidence.UnbindMode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("missing_binding_object_evidence"), TEXT("delegate clear requires ContextEvidence.event_delegate.unbind_mode=all."), OutMissingDetail, OutMessage);
		}
		if (!OutEvidence.HandlerName.IsEmpty()
			|| !OutEvidence.HandlerFunctionPath.IsEmpty()
			|| !OutEvidence.HandlerSourceCluster.IsEmpty()
			|| !OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return UGraphWriteActionEvidenceUtils::SetMissingResult(TEXT("handler_not_allowed_for_clear"), TEXT("delegate clear must not include handler evidence."), OutMissingDetail, OutMessage);
		}
	}

	if (!UGraphWriteActionEvidenceUtils::ResolveDelegateProperty(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	const bool bCanResolveComponentBinding =
		!OutEvidence.ComponentPath.IsEmpty()
		&& !OutEvidence.ComponentBindingOwnerClassPath.IsEmpty()
		&& !OutEvidence.ComponentBindingFieldPath.IsEmpty();
	if ((SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent || bCanResolveComponentBinding)
		&& !UGraphWriteActionEvidenceUtils::ResolveComponentBindingProperty(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	if (UGraphWriteActionEvidenceUtils::RequiresResolvedHandlerForOp(SemanticKind, OutEvidence.DelegateOperation)
		&& !UGraphWriteActionEvidenceUtils::ResolveHandlerFunction(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	return true;
}
