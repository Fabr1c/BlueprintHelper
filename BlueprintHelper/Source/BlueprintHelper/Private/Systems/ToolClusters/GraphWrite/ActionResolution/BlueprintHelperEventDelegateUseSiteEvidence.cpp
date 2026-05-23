#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"

#include "Engine/Blueprint.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace
{
static FString EvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
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
			TEXT("delegate_owner_class_unresolved"),
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
			TEXT("delegate_property_unresolved"),
			FString::Printf(TEXT("Could not resolve multicast delegate property '%s' on '%s'."), *Evidence.DelegatePropertyName, *Evidence.DelegateOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.DelegateProperty, Evidence.DelegatePropertyPath))
	{
		return Missing(
			TEXT("delegate_property_path_mismatch"),
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
			TEXT("component_owner_class_unresolved"),
			FString::Printf(TEXT("Could not resolve component owner class '%s'."), *Evidence.ComponentBindingOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}

	Evidence.ComponentBindingProperty = FindPropertyOnClass<FObjectProperty>(
		ComponentOwnerClass,
		Evidence.ComponentPath);
	if (!Evidence.ComponentBindingProperty)
	{
		return Missing(
			TEXT("component_property_unresolved"),
			FString::Printf(TEXT("Could not resolve component binding property '%s' on '%s'."), *Evidence.ComponentPath, *Evidence.ComponentBindingOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.ComponentBindingProperty, Evidence.ComponentBindingFieldPath))
	{
		return Missing(
			TEXT("component_property_path_mismatch"),
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
	UClass* HandlerScopeClass = FindClassByPath(Evidence.HandlerScopeClassPath);
	if (!HandlerScopeClass)
	{
		return Missing(
			TEXT("handler_scope_unresolved"),
			FString::Printf(TEXT("Could not resolve handler scope class '%s'."), *Evidence.HandlerScopeClassPath),
			OutMissingDetail,
			OutMessage);
	}

	Evidence.HandlerFunction = HandlerScopeClass->FindFunctionByName(FName(*Evidence.HandlerName.TrimStartAndEnd()));
	if (!Evidence.HandlerFunction)
	{
		return Missing(
			TEXT("handler_function_unresolved"),
			FString::Printf(TEXT("Could not resolve existing handler function '%s' on '%s'."), *Evidence.HandlerName, *Evidence.HandlerScopeClassPath),
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
	OutEvidence.DelegateOperation = NormalizeDelegateOperation(EvidenceValue(Request, TEXT("delegate_operation")));
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
	OutEvidence.ComponentPath = EvidenceValue(Request, TEXT("component_path"));
	OutEvidence.ComponentBindingOwnerClassPath = EvidenceValue(Request, TEXT("component_binding_owner_class_path"));
	OutEvidence.ComponentBindingFieldPath = EvidenceValue(Request, TEXT("component_binding_field_path"));
	OutEvidence.BindingObjectPath = EvidenceValue(Request, TEXT("binding_object_path"));
	OutEvidence.HandlerName = EvidenceValue(Request, TEXT("handler_name"));
	OutEvidence.HandlerScopeClassPath = EvidenceValue(Request, TEXT("handler_scope_class_path"));
	OutEvidence.UnbindMode = EvidenceValue(Request, TEXT("unbind_mode"));

	if (SemanticKind != EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& SemanticKind != EBlueprintHelperActionSemanticKind::Delegate)
	{
		return Missing(
			TEXT("semantic_kind_unsupported"),
			FString::Printf(TEXT("EventDelegate use-site evidence does not support semantic '%s'."), *FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind)),
			OutMissingDetail,
			OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate)
	{
		if (OutEvidence.DelegateOperation.IsEmpty())
		{
			return Missing(TEXT("delegate_operation_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_operation."), OutMissingDetail, OutMessage);
		}
		if (!IsSupportedDelegateOperation(OutEvidence.DelegateOperation))
		{
			return Missing(
				TEXT("delegate_operation_unsupported"),
				FString::Printf(TEXT("Unsupported delegate_operation '%s'."), *OutEvidence.DelegateOperation),
				OutMissingDetail,
				OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		&& OutEvidence.ComponentPath.IsEmpty())
	{
		return Missing(TEXT("component_missing"), TEXT("Component-bound event resolution requires ContextEvidence.component_path."), OutMissingDetail, OutMessage);
	}
	if (RequiresBindingObject(SemanticKind) && OutEvidence.BindingObjectPath.IsEmpty())
	{
		return Missing(TEXT("binding_object_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.binding_object_path."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateName.IsEmpty())
	{
		return Missing(TEXT("delegate_name_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_name or semantic delegate query."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateSignature.IsEmpty())
	{
		return Missing(TEXT("delegate_signature_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_signature."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegateOwnerClassPath.IsEmpty())
	{
		return Missing(TEXT("delegate_owner_class_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_owner_class_path."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyName.IsEmpty())
	{
		return Missing(TEXT("delegate_property_name_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_property_name."), OutMissingDetail, OutMessage);
	}
	if (OutEvidence.DelegatePropertyPath.IsEmpty())
	{
		return Missing(TEXT("delegate_property_path_missing"), TEXT("Delegate use-site resolution requires ContextEvidence.delegate_property_path."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.ComponentBindingOwnerClassPath.IsEmpty())
		{
			return Missing(TEXT("component_owner_class_missing"), TEXT("Component-bound event resolution requires ContextEvidence.component_binding_owner_class_path."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.ComponentBindingFieldPath.IsEmpty())
		{
			return Missing(TEXT("component_binding_field_missing"), TEXT("Component-bound event resolution requires ContextEvidence.component_binding_field_path."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& RequiresHandler(OutEvidence.DelegateOperation))
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return Missing(TEXT("handler_missing"), TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.handler_name."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return Missing(TEXT("handler_scope_missing"), TEXT("Delegate bind/assign/unbind resolution requires ContextEvidence.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		if (OutEvidence.HandlerName.IsEmpty())
		{
			return Missing(TEXT("handler_missing"), TEXT("Component-bound event resolution requires ContextEvidence.handler_name."), OutMissingDetail, OutMessage);
		}
		if (OutEvidence.HandlerScopeClassPath.IsEmpty())
		{
			return Missing(TEXT("handler_scope_missing"), TEXT("Component-bound event resolution requires ContextEvidence.handler_scope_class_path."), OutMissingDetail, OutMessage);
		}
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase)
		&& !OutEvidence.UnbindMode.Equals(TEXT("single"), ESearchCase::IgnoreCase))
	{
		return Missing(TEXT("unbind_mode_single_missing"), TEXT("delegate unbind requires ContextEvidence.unbind_mode=single."), OutMissingDetail, OutMessage);
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& OutEvidence.DelegateOperation.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		if (!OutEvidence.UnbindMode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			return Missing(TEXT("unbind_mode_all_missing"), TEXT("delegate clear requires ContextEvidence.unbind_mode=all."), OutMissingDetail, OutMessage);
		}
		if (!OutEvidence.HandlerName.IsEmpty())
		{
			return Missing(TEXT("delegate_clear_handler_forbidden"), TEXT("delegate clear must not include handler_name."), OutMissingDetail, OutMessage);
		}
	}

	if (!ResolveDelegateProperty(OutEvidence, OutMissingDetail, OutMessage))
	{
		return false;
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
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
