#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
static FString EvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Key)
{
	const FString NamespacedKey = Key.StartsWith(TEXT("event_delegate."))
		? Key
		: FString::Printf(TEXT("event_delegate.%s"), *Key);
	if (const FString* Value = Request.ContextEvidence.Find(NamespacedKey))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static FString FirstNonEmpty(const TArray<FString>& Values)
{
	for (const FString& Value : Values)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return Trimmed;
		}
	}
	return FString();
}

static bool Missing(
	const FString& Detail,
	const FString& Message,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	OutMissingDetail = Detail;
	OutMessage = Message;
	return false;
}

static UClass* FindClassByPath(const FString& ClassPath)
{
	if (ClassPath.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	return FindObject<UClass>(nullptr, *ClassPath.TrimStartAndEnd());
}

template <typename TProperty>
static TProperty* FindPropertyOnClass(UClass* OwnerClass, const FString& PropertyName)
{
	if (!OwnerClass || PropertyName.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	for (UClass* Class = OwnerClass; Class; Class = Class->GetSuperClass())
	{
		if (TProperty* Property = FindFProperty<TProperty>(Class, FName(*PropertyName.TrimStartAndEnd())))
		{
			return Property;
		}
	}
	return nullptr;
}

static bool PathMatches(const FField* Field, const FString& ExpectedPath)
{
	return Field
		&& !ExpectedPath.TrimStartAndEnd().IsEmpty()
		&& Field->GetPathName().Equals(ExpectedPath.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

static bool ResolveDelegateProperty(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	UClass* DelegateOwnerClass = FindClassByPath(Evidence.DelegateOwnerClassPath);
	if (!DelegateOwnerClass)
	{
		return Missing(
			TEXT("missing_delegate_property_evidence"),
			FString::Printf(TEXT("Could not resolve delegate owner class '%s'."), *Evidence.DelegateOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}

	Evidence.DelegateProperty = FindPropertyOnClass<FMulticastDelegateProperty>(
		DelegateOwnerClass,
		Evidence.DelegatePropertyName);
	if (!Evidence.DelegateProperty)
	{
		return Missing(
			TEXT("missing_delegate_property_evidence"),
			FString::Printf(TEXT("Could not resolve multicast delegate property '%s' on '%s'."), *Evidence.DelegatePropertyName, *Evidence.DelegateOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.DelegateProperty, Evidence.DelegatePropertyPath))
	{
		return Missing(
			TEXT("missing_delegate_property_evidence"),
			FString::Printf(TEXT("Resolved delegate property path '%s' does not match projected path '%s'."), *Evidence.DelegateProperty->GetPathName(), *Evidence.DelegatePropertyPath),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

static bool ResolveComponentBindingProperty(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	UClass* ComponentOwnerClass = FindClassByPath(Evidence.ComponentBindingOwnerClassPath);
	if (!ComponentOwnerClass)
	{
		return Missing(
			TEXT("missing_binding_object_evidence"),
			FString::Printf(TEXT("Could not resolve component owner class '%s'."), *Evidence.ComponentBindingOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}

	Evidence.ComponentBindingProperty = FindPropertyOnClass<FObjectPropertyBase>(
		ComponentOwnerClass,
		Evidence.ComponentPath);
	if (!Evidence.ComponentBindingProperty)
	{
		return Missing(
			TEXT("missing_binding_object_evidence"),
			FString::Printf(TEXT("Could not resolve component binding property '%s' on '%s'."), *Evidence.ComponentPath, *Evidence.ComponentBindingOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.ComponentBindingProperty, Evidence.ComponentBindingFieldPath))
	{
		return Missing(
			TEXT("missing_binding_object_evidence"),
			FString::Printf(TEXT("Resolved component property path '%s' does not match projected path '%s'."), *Evidence.ComponentBindingProperty->GetPathName(), *Evidence.ComponentBindingFieldPath),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

static bool ResolveHandlerFunction(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	Evidence.HandlerFunction = FindObject<UFunction>(nullptr, *Evidence.HandlerFunctionPath.TrimStartAndEnd());
	if (!Evidence.HandlerFunction)
	{
		return Missing(
			TEXT("missing_handler_evidence"),
			FString::Printf(TEXT("Could not resolve projected handler function '%s'."), *Evidence.HandlerFunctionPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!Evidence.HandlerName.IsEmpty()
		&& !Evidence.HandlerFunction->GetName().Equals(Evidence.HandlerName, ESearchCase::IgnoreCase))
	{
		return Missing(
			TEXT("missing_handler_evidence"),
			FString::Printf(TEXT("Projected handler function '%s' does not match handler_name '%s'."), *Evidence.HandlerFunction->GetName(), *Evidence.HandlerName),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

static FString NormalizeDelegateOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

static bool IsSupportedDelegateOperation(const FString& Operation)
{
	const FString Normalized = NormalizeDelegateOperation(Operation);
	return Normalized == TEXT("bind")
		|| Normalized == TEXT("assign")
		|| Normalized == TEXT("unbind")
		|| Normalized == TEXT("call")
		|| Normalized == TEXT("clear");
}

static bool RequiresBindingObject(EBlueprintHelperActionSemanticKind SemanticKind)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::Delegate;
}

static bool RequiresHandler(const FString& Operation)
{
	const FString Normalized = NormalizeDelegateOperation(Operation);
	return Normalized == TEXT("bind")
		|| Normalized == TEXT("assign")
		|| Normalized == TEXT("unbind");
}

static bool RequiresResolvedHandler(
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Operation)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| RequiresHandler(Operation);
}
}

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
	OutEvidence.DelegateOperation = NormalizeDelegateOperation(EvidenceValue(Request, TEXT("operation")));
	OutEvidence.DelegateName = FirstNonEmpty({
		EvidenceValue(Request, TEXT("delegate_name")),
		Request.Semantic.PropertyPath,
		Request.Semantic.Query
	});
	OutEvidence.DelegateOwnerClassPath = EvidenceValue(Request, TEXT("delegate_owner_class_path"));
	OutEvidence.DelegatePropertyName = FirstNonEmpty({
		EvidenceValue(Request, TEXT("delegate_property_name")),
		OutEvidence.DelegateName
	});
	OutEvidence.DelegatePropertyPath = EvidenceValue(Request, TEXT("delegate_property_path"));
	OutEvidence.DelegateSignature = EvidenceValue(Request, TEXT("delegate_signature"));
	OutEvidence.DelegateSignatureFunctionPath = EvidenceValue(Request, TEXT("delegate_signature_function_path"));
	OutEvidence.ComponentPath = FirstNonEmpty({
		EvidenceValue(Request, TEXT("component_property_name")),
		EvidenceValue(Request, TEXT("component_path")),
		SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
			? EvidenceValue(Request, TEXT("binding_object_path"))
			: FString()
	});
	OutEvidence.ComponentBindingOwnerClassPath = EvidenceValue(Request, TEXT("component_binding_owner_class_path"));
	OutEvidence.ComponentBindingFieldPath = EvidenceValue(Request, TEXT("component_binding_field_path"));
	OutEvidence.ComponentClassPath = EvidenceValue(Request, TEXT("component_class_path"));
	OutEvidence.BindingObjectKind = EvidenceValue(Request, TEXT("binding_object_kind"));
	OutEvidence.BindingObjectEvidenceId = EvidenceValue(Request, TEXT("binding_object_evidence_id"));
	OutEvidence.BindingObjectPath = EvidenceValue(Request, TEXT("binding_object_path"));
	OutEvidence.BindingObjectProducerStatementId = EvidenceValue(Request, TEXT("binding_object_statement_id"));
	OutEvidence.BindingObjectNodeGuid = EvidenceValue(Request, TEXT("binding_object_node_guid"));
	OutEvidence.BindingObjectPinName = EvidenceValue(Request, TEXT("binding_object_pin_name"));
	OutEvidence.BindingObjectError = EvidenceValue(Request, TEXT("binding_object_error"));
	OutEvidence.HandlerName = EvidenceValue(Request, TEXT("handler_name"));
	OutEvidence.HandlerScopeClassPath = EvidenceValue(Request, TEXT("handler_scope_class_path"));
	OutEvidence.HandlerFunctionPath = EvidenceValue(Request, TEXT("handler_function_path"));
	OutEvidence.HandlerSourceCluster = EvidenceValue(Request, TEXT("handler_source_cluster"));
	OutEvidence.SignatureEvidenceId = EvidenceValue(Request, TEXT("signature_evidence_id"));
	OutEvidence.UnbindMode = EvidenceValue(Request, TEXT("unbind_mode"));
	OutEvidence.DuplicatePolicy = EvidenceValue(Request, TEXT("duplicate_policy"));
	OutEvidence.AssignFactory = EvidenceValue(Request, TEXT("assign_factory"));

	if (SemanticKind != EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& SemanticKind != EBlueprintHelperActionSemanticKind::Delegate)
	{
		return Missing(
			TEXT("invalid_delegate_operation"),
			FString::Printf(TEXT("EventDelegate use-site evidence does not support semantic '%s'."), *FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind)),
			OutMissingDetail,
			OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate)
	{
		if (!OutEvidence.BindingObjectError.IsEmpty())
		{
			return Missing(
				OutEvidence.BindingObjectError,
				FString::Printf(TEXT("EventDelegate binding object projection failed: %s."), *OutEvidence.BindingObjectError),
				OutMissingDetail,
				OutMessage);
		}
		if (OutEvidence.DelegateOperation.IsEmpty())
		{
			return Missing(TEXT("invalid_delegate_operation"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.operation."), OutMissingDetail, OutMessage);
		}
		if (!IsSupportedDelegateOperation(OutEvidence.DelegateOperation))
		{
			return Missing(
				TEXT("invalid_delegate_operation"),
				FString::Printf(TEXT("Unsupported delegate_operation '%s'."), *OutEvidence.DelegateOperation),
				OutMissingDetail,
				OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& OutEvidence.ComponentPath.IsEmpty())
	{
		return Missing(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_property_name."), OutMissingDetail, OutMessage);
	}
	if (RequiresBindingObject(SemanticKind)
		&& OutEvidence.BindingObjectKind.IsEmpty())
	{
		return Missing(TEXT("missing_binding_object_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.binding_object_kind."), OutMissingDetail, OutMessage);
	}
	if (RequiresBindingObject(SemanticKind)
		&& OutEvidence.BindingObjectEvidenceId.IsEmpty())
	{
		return Missing(TEXT("missing_binding_object_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.binding_object_evidence_id."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateName.IsEmpty())
	{
		return Missing(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_name or semantic delegate query."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateOwnerClassPath.IsEmpty())
	{
		return Missing(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_owner_class_path."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyName.IsEmpty())
	{
		return Missing(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_name."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyPath.IsEmpty())
	{
		return Missing(TEXT("missing_delegate_property_evidence"), TEXT("Delegate use-site resolution requires ContextEvidence.event_delegate.delegate_property_path."), OutMissingDetail, OutMessage);
	}
	if (!OutEvidence.DelegateOperation.Equals(TEXT("clear"), ESearchCase::IgnoreCase)
		&& OutEvidence.DelegateSignatureFunctionPath.IsEmpty())
	{
		return Missing(TEXT("missing_signature_evidence"), TEXT("EventDelegate use-site resolution requires ContextEvidence.event_delegate.delegate_signature_function_path."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.ComponentBindingOwnerClassPath.IsEmpty())
		{
			return Missing(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_binding_owner_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.ComponentBindingFieldPath.IsEmpty())
		{
			return Missing(TEXT("missing_binding_object_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.component_binding_field_path."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& RequiresHandler(OutEvidence.DelegateOperation))
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return Missing(
				OutEvidence.DelegateOperation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase) ? TEXT("handler_required_for_unbind") : TEXT("missing_handler_evidence"),
				TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.event_delegate.handler_name."),
				OutMissingDetail,
				OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.event_delegate.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerFunctionPath.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_function_path from BlueprintSignature."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerSourceCluster.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_source_cluster."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return Missing(TEXT("missing_signature_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.signature_evidence_id."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.handler_name."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("Component-bound event resolution requires ContextEvidence.event_delegate.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerFunctionPath.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_function_path from BlueprintSignature."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerSourceCluster.IsEmpty())
		{
			return Missing(TEXT("missing_handler_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.handler_source_cluster."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return Missing(TEXT("missing_signature_evidence"), TEXT("EventDelegate resolution requires projected ContextEvidence.event_delegate.signature_evidence_id."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase)
		&& !OutEvidence.UnbindMode.Equals(TEXT("single"), ESearchCase::IgnoreCase))
	{
		return Missing(TEXT("handler_required_for_unbind"), TEXT("delegate unbind requires ContextEvidence.event_delegate.unbind_mode=single."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		if (!OutEvidence.UnbindMode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			return Missing(TEXT("missing_binding_object_evidence"), TEXT("delegate clear requires ContextEvidence.event_delegate.unbind_mode=all."), OutMissingDetail, OutMessage);
		}
		if (!OutEvidence.HandlerName.IsEmpty()
			|| !OutEvidence.HandlerFunctionPath.IsEmpty()
			|| !OutEvidence.HandlerSourceCluster.IsEmpty()
			|| !OutEvidence.SignatureEvidenceId.IsEmpty())
		{
			return Missing(TEXT("handler_not_allowed_for_clear"), TEXT("delegate clear must not include handler evidence."), OutMissingDetail, OutMessage);
		}
	}

	if (!ResolveDelegateProperty(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	const bool bCanResolveComponentBinding =
		!OutEvidence.ComponentPath.IsEmpty()
		&& !OutEvidence.ComponentBindingOwnerClassPath.IsEmpty()
		&& !OutEvidence.ComponentBindingFieldPath.IsEmpty();
	if ((SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent || bCanResolveComponentBinding)
		&& !ResolveComponentBindingProperty(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	if (RequiresResolvedHandler(SemanticKind, OutEvidence.DelegateOperation)
		&& !ResolveHandlerFunction(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	return true;
}
