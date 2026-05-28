#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"

// ============================================================================
// BlueprintHelperGenericOpsEvidence.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::GetEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Clean(*Value);
	}
	if (const FString* Value = Request.Semantic.DefaultValues.Find(Key))
	{
		return Clean(*Value);
	}
	return FString();
}

void UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Prefix,
	TMap<FString, FString>& OutFacts)
{
	for (const TPair<FString, FString>& Pair : Request.ContextEvidence)
	{
		const FString Value = Clean(Pair.Value);
		if (Pair.Key.StartsWith(Prefix, ESearchCase::IgnoreCase) && !Value.IsEmpty())
		{
			OutFacts.FindOrAdd(Pair.Key) = Value;
		}
	}
	for (const TPair<FString, FString>& Pair : Request.Semantic.DefaultValues)
	{
		const FString Value = Clean(Pair.Value);
		if (Pair.Key.StartsWith(Prefix, ESearchCase::IgnoreCase) && !Value.IsEmpty())
		{
			OutFacts.FindOrAdd(Pair.Key) = Value;
		}
	}
}

TArray<FString> UGraphWriteActionEvidenceUtils::SplitList(const FString& Value)
{
	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part = Clean(Part);
	}
	Parts.RemoveAll([](const FString& Part) { return Part.IsEmpty(); });
	return Parts;
}

bool UGraphWriteActionEvidenceUtils::FailMissing(
	const TCHAR* Key,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutErrorCode = FString::Printf(TEXT("missing_evidence.%s"), Key);
	OutMessage = FString::Printf(TEXT("GenericOps evidence requires key %s."), Key);
	return false;
}

bool UGraphWriteActionEvidenceUtils::IsLatentScheduleOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("latent_or_async_node"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("delay"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("retriggerable_delay"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("delay_until_next_tick"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("generic_latent_function_call"), ESearchCase::IgnoreCase);
}

// ============================================================================
// BlueprintHelperOpCallableEvidence.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::GetOpCallableEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

FString UGraphWriteActionEvidenceUtils::ReadRequestedOperationId(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString FunctionOperation = Request.Semantic.FunctionOperation.TrimStartAndEnd();
	if (FunctionOperation.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
	{
		return FunctionOperation;
	}

	const FString EvidenceOperation = GetOpCallableEvidenceValue(Request, TEXT("op.operation_id"));
	if (!EvidenceOperation.IsEmpty())
	{
		return EvidenceOperation;
	}

	return FString();
}

// ============================================================================
// BlueprintHelperProjectedSpawnerEvidence.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

// ============================================================================
// BlueprintHelperEventDelegatePolicy.cpp
// ============================================================================

FBlueprintHelperEventDelegatePolicyDecision UGraphWriteActionEvidenceUtils::AllowEventDelegate()
{
	FBlueprintHelperEventDelegatePolicyDecision Decision;
	Decision.bAllowed = true;
	Decision.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	return Decision;
}

FBlueprintHelperEventDelegatePolicyDecision UGraphWriteActionEvidenceUtils::BlockEventDelegate(
	const FString& ErrorCode,
	const FString& Message,
	const EBlueprintHelperActionResolutionStatus Status)
{
	FBlueprintHelperEventDelegatePolicyDecision Decision;
	Decision.bAllowed = false;
	Decision.Status = Status;
	Decision.ErrorCode = ErrorCode;
	Decision.Message = Message;
	return Decision;
}

FString UGraphWriteActionEvidenceUtils::GetDirectEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

bool UGraphWriteActionEvidenceUtils::IsTrueEvidenceValue(const FString& Value)
{
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionEvidenceUtils::IsFalseEvidenceValue(const FString& Value)
{
	return Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("0"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("no"), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionEvidenceUtils::HasExistingBindingEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	return !GetDirectEvidenceValue(Request, TEXT("event_delegate.existing_binding_evidence_id")).IsEmpty()
		|| IsTrueEvidenceValue(GetDirectEvidenceValue(Request, TEXT("event_delegate.existing_binding")));
}

// ============================================================================
// BlueprintHelperEventDelegateUseSiteEvidence.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::GetNamespacedDelegateEvidenceValue(
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

FString UGraphWriteActionEvidenceUtils::FirstNonEmpty(const TArray<FString>& Values)
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

bool UGraphWriteActionEvidenceUtils::SetMissingResult(
	const FString& Detail,
	const FString& Message,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	OutMissingDetail = Detail;
	OutMessage = Message;
	return false;
}

UClass* UGraphWriteActionEvidenceUtils::FindClassByPath(const FString& ClassPath)
{
	if (ClassPath.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	return FindObject<UClass>(nullptr, *ClassPath.TrimStartAndEnd());
}

bool UGraphWriteActionEvidenceUtils::PathMatches(
	const FField* Field,
	const FString& ExpectedPath)
{
	return Field
		&& !ExpectedPath.TrimStartAndEnd().IsEmpty()
		&& Field->GetPathName().Equals(ExpectedPath.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionEvidenceUtils::ResolveDelegateProperty(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	UClass* DelegateOwnerClass = FindClassByPath(Evidence.DelegateOwnerClassPath);
	if (!DelegateOwnerClass)
	{
		return SetMissingResult(
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
		return SetMissingResult(
			TEXT("missing_delegate_property_evidence"),
			FString::Printf(TEXT("Could not resolve multicast delegate property '%s' on '%s'."), *Evidence.DelegatePropertyName, *Evidence.DelegateOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.DelegateProperty, Evidence.DelegatePropertyPath))
	{
		return SetMissingResult(
			TEXT("missing_delegate_property_evidence"),
			FString::Printf(TEXT("Resolved delegate property path '%s' does not match projected path '%s'."), *Evidence.DelegateProperty->GetPathName(), *Evidence.DelegatePropertyPath),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

bool UGraphWriteActionEvidenceUtils::ResolveComponentBindingProperty(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	UClass* ComponentOwnerClass = FindClassByPath(Evidence.ComponentBindingOwnerClassPath);
	if (!ComponentOwnerClass)
	{
		return SetMissingResult(
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
		return SetMissingResult(
			TEXT("missing_binding_object_evidence"),
			FString::Printf(TEXT("Could not resolve component binding property '%s' on '%s'."), *Evidence.ComponentPath, *Evidence.ComponentBindingOwnerClassPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!PathMatches(Evidence.ComponentBindingProperty, Evidence.ComponentBindingFieldPath))
	{
		return SetMissingResult(
			TEXT("missing_binding_object_evidence"),
			FString::Printf(TEXT("Resolved component property path '%s' does not match projected path '%s'."), *Evidence.ComponentBindingProperty->GetPathName(), *Evidence.ComponentBindingFieldPath),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

bool UGraphWriteActionEvidenceUtils::ResolveHandlerFunction(
	FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutMissingDetail,
	FString& OutMessage)
{
	Evidence.HandlerFunction = FindObject<UFunction>(nullptr, *Evidence.HandlerFunctionPath.TrimStartAndEnd());
	if (!Evidence.HandlerFunction)
	{
		return SetMissingResult(
			TEXT("missing_handler_evidence"),
			FString::Printf(TEXT("Could not resolve projected handler function '%s'."), *Evidence.HandlerFunctionPath),
			OutMissingDetail,
			OutMessage);
	}
	if (!Evidence.HandlerName.IsEmpty()
		&& !Evidence.HandlerFunction->GetName().Equals(Evidence.HandlerName, ESearchCase::IgnoreCase))
	{
		return SetMissingResult(
			TEXT("missing_handler_evidence"),
			FString::Printf(TEXT("Projected handler function '%s' does not match handler_name '%s'."), *Evidence.HandlerFunction->GetName(), *Evidence.HandlerName),
			OutMissingDetail,
			OutMessage);
	}
	return true;
}

FString UGraphWriteActionEvidenceUtils::NormalizeDelegateOperationToken(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

bool UGraphWriteActionEvidenceUtils::IsSupportedDelegateOperation(const FString& Operation)
{
	const FString Normalized = NormalizeDelegateOperationToken(Operation);
	return Normalized == TEXT("bind")
		|| Normalized == TEXT("assign")
		|| Normalized == TEXT("unbind")
		|| Normalized == TEXT("call")
		|| Normalized == TEXT("clear");
}

bool UGraphWriteActionEvidenceUtils::RequiresBindingObject(EBlueprintHelperActionSemanticKind SemanticKind)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::Delegate;
}

bool UGraphWriteActionEvidenceUtils::RequiresHandlerForOperation(const FString& Operation)
{
	const FString Normalized = NormalizeDelegateOperationToken(Operation);
	return Normalized == TEXT("bind")
		|| Normalized == TEXT("assign")
		|| Normalized == TEXT("unbind");
}

bool UGraphWriteActionEvidenceUtils::RequiresResolvedHandlerForOp(
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Operation)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| RequiresHandlerForOperation(Operation);
}

// ============================================================================
// BlueprintHelperArrayTypedPinEvidenceGuard.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::GetMapEvidenceValue(
	const TMap<FString, FString>& Evidence,
	const FString& Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

FString UGraphWriteActionEvidenceUtils::NormalizeToken(const FString& Token)
{
	return Token.TrimStartAndEnd().ToLower();
}

bool UGraphWriteActionEvidenceUtils::ElementCategoryRequiresObjectPath(const FString& Category)
{
	return Category == TEXT("struct")
		|| Category == TEXT("object")
		|| Category == TEXT("class")
		|| Category == TEXT("interface")
		|| Category == TEXT("enum")
		|| Category == TEXT("softobject")
		|| Category == TEXT("softclass");
}

FString UGraphWriteActionEvidenceUtils::BuildArrayElementIdentity(FBlueprintHelperCallFunctionPinType& PinType)
{
	const FString Category = NormalizeToken(PinType.Category);
	const FString Container = NormalizeToken(PinType.ContainerType);
	if (Category == TEXT("array") || Category == TEXT("tarray"))
	{
		PinType.ContainerType = TEXT("array");
		PinType.Category = PinType.SubCategory;
		PinType.SubCategory.Reset();
	}
	else if (Container == TEXT("array") || Container == TEXT("tarray"))
	{
		PinType.ContainerType = TEXT("array");
	}
	else
	{
		return FString();
	}

	const FString ElementCategory = NormalizeToken(PinType.Category);
	const FString ElementSubCategory = NormalizeToken(PinType.SubCategory);
	const FString ElementObjectPath = NormalizeToken(PinType.ObjectPath);
	if (ElementCategory.IsEmpty()
		|| ElementCategory == TEXT("wildcard")
		|| ElementSubCategory == TEXT("wildcard")
		|| ElementObjectPath == TEXT("wildcard"))
	{
		return FString();
	}
	if (ElementCategoryRequiresObjectPath(ElementCategory) && ElementObjectPath.IsEmpty())
	{
		return FString();
	}

	FString Identity = FString::Printf(TEXT("category=%s"), *ElementCategory);
	if (!ElementSubCategory.IsEmpty())
	{
		Identity += FString::Printf(TEXT("|subcategory=%s"), *ElementSubCategory);
	}
	if (!ElementObjectPath.IsEmpty())
	{
		Identity += FString::Printf(TEXT("|object=%s"), *ElementObjectPath);
	}
	return Identity;
}

FBlueprintHelperArrayTypedPinEvidenceGuardResult UGraphWriteActionEvidenceUtils::FailArrayPinEvidence(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperArrayTypedPinEvidenceGuardResult Result;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

// ============================================================================
// BlueprintHelperFieldCapabilityTypes.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

FBlueprintHelperFieldCapabilitySpec UGraphWriteActionEvidenceUtils::MakeFieldCapabilitySpec(
	const TCHAR* Id,
	EBlueprintHelperFieldCapabilityPriority Priority,
	EBlueprintHelperFieldCapabilityRootKind RootKind,
	EBlueprintHelperFieldCapabilityAccessMode AccessMode,
	const TCHAR* FieldOperation,
	const TCHAR* FieldScope,
	const TCHAR* ExpectedNodeFamily,
	const TCHAR* ExpectedNodeClass,
	const bool bRequiresOwnerClass,
	const bool bRequiresFunctionScope,
	const bool bRequiresTargetPin,
	const bool bRequiresPropertyPath,
	const bool bProducesExecPins)
{
	FBlueprintHelperFieldCapabilitySpec Spec;
	Spec.Id = Id;
	Spec.Priority = Priority;
	Spec.RootKind = RootKind;
	Spec.AccessMode = AccessMode;
	Spec.FieldOperation = FieldOperation;
	Spec.FieldScope = FieldScope;
	Spec.ExpectedNodeFamily = ExpectedNodeFamily;
	Spec.ExpectedNodeClass = ExpectedNodeClass;
	Spec.bFirstClassStatement = true;
	Spec.bRequiresOwnerClass = bRequiresOwnerClass;
	Spec.bRequiresFunctionScope = bRequiresFunctionScope;
	Spec.bRequiresTargetPin = bRequiresTargetPin;
	Spec.bRequiresPropertyPath = bRequiresPropertyPath;
	Spec.bProducesExecPins = bProducesExecPins;
	return Spec;
}

FBlueprintHelperFieldCapabilitySpec UGraphWriteActionEvidenceUtils::MakeRejectedFieldCapabilitySpec(
	const TCHAR* Id,
	EBlueprintHelperFieldCapabilityPriority Priority,
	EBlueprintHelperFieldCapabilityAccessMode AccessMode,
	const TCHAR* FieldOperation,
	const TCHAR* FieldScope,
	const TCHAR* RejectReason)
{
	FBlueprintHelperFieldCapabilitySpec Spec;
	Spec.Id = Id;
	Spec.Priority = Priority;
	Spec.RootKind = EBlueprintHelperFieldCapabilityRootKind::Unsupported;
	Spec.AccessMode = AccessMode;
	Spec.FieldOperation = FieldOperation;
	Spec.FieldScope = FieldScope;
	Spec.RejectReason = RejectReason;
	return Spec;
}

const TArray<FBlueprintHelperFieldCapabilitySpec>& UGraphWriteActionEvidenceUtils::GetAllFieldCapabilitySpecs()
{
	static const TArray<FBlueprintHelperFieldCapabilitySpec> Specs = {
		MakeFieldCapabilitySpec(TEXT("field.member_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.member_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeFieldCapabilitySpec(TEXT("field.inherited_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.inherited_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, false, false, true),
		MakeFieldCapabilitySpec(TEXT("field.sparse_data_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::SparseData, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.function_param_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::FunctionParam, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.local_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.local_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, true, false, false, true),
		MakeFieldCapabilitySpec(TEXT("field.object_pin_member_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("field_access"), TEXT("variable_get_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, false, false),
		MakeFieldCapabilitySpec(TEXT("field.object_pin_member_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("field_access"), TEXT("variable_set_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, false, true),
		MakeFieldCapabilitySpec(TEXT("field.component_ref_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("component_ref"), TEXT("component_variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeFieldCapabilitySpec(TEXT("field.component_ref_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("component_ref"), TEXT("component_variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeFieldCapabilitySpec(TEXT("field.component_property_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("field_access"), TEXT("component_property_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, true, false),
		MakeFieldCapabilitySpec(TEXT("field.component_property_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("field_access"), TEXT("component_property_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, true, true),
		MakeFieldCapabilitySpec(TEXT("field.struct_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("property_path"), TEXT("break_struct"), TEXT("/Script/BlueprintGraph.K2Node_BreakStruct"), false, false, false, true, false),
		MakeFieldCapabilitySpec(TEXT("field.struct_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("property_path"), TEXT("set_fields_in_struct"), TEXT("/Script/BlueprintGraph.K2Node_SetFieldsInStruct"), false, false, false, true, true),
		MakeFieldCapabilitySpec(TEXT("field.nested_property_path"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath, EBlueprintHelperFieldCapabilityAccessMode::ReadWritePath, TEXT("get_property"), TEXT("property_path"), TEXT("property_path_fragment"), TEXT("BlueprintHelper.Field.PropertyPathFragment"), false, false, false, true, false),
		MakeRejectedFieldCapabilitySpec(TEXT("field.drag_get"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("ui_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.drag_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("ui_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.pin_drag_get"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("ui_pin_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.pin_drag_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("ui_pin_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.split_struct_pin_support"), EBlueprintHelperFieldCapabilityPriority::SupportOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("support"), TEXT("property_path"), TEXT("support_only_not_user_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.recombine_struct_pin_support"), EBlueprintHelperFieldCapabilityPriority::SupportOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("support"), TEXT("property_path"), TEXT("support_only_not_user_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("control.function_return_write"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("function_return"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("function.selected_component_call"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("call"), TEXT("selected_component"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("component.add_component_node"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("spawn"), TEXT("component"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.unsupported_path_diagnostic"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("diagnostic"), TEXT("property_path"), TEXT("diagnostic_only_not_success_capability")),
		MakeRejectedFieldCapabilitySpec(TEXT("field.by_ref_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("by_ref"), TEXT("unsupported_by_ref_set_deferred"))
	};

	return Specs;
}

const FBlueprintHelperFieldCapabilitySpec* UGraphWriteActionEvidenceUtils::FindKnownFieldCapabilitySpec(
	const FString& CapabilityId,
	const bool bFirstClassOnly)
{
	const FString NormalizedCapabilityId = NormalizeFieldCapabilityToken(CapabilityId);
	if (NormalizedCapabilityId.IsEmpty())
	{
		return nullptr;
	}

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllFieldCapabilitySpecs())
	{
		if (bFirstClassOnly && !Spec.bFirstClassStatement)
		{
			continue;
		}

		if (NormalizeFieldCapabilityToken(Spec.Id) == NormalizedCapabilityId)
		{
			return &Spec;
		}
	}

	return nullptr;
}

// ============================================================================
// BlueprintHelperContainerActionVocabulary.cpp
// ============================================================================

FString UGraphWriteActionEvidenceUtils::NormalizeContainerActionToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

FBlueprintHelperContainerActionRoleBinding UGraphWriteActionEvidenceUtils::BindInputRole(
	const TCHAR* RoleName,
	const TCHAR* FunctionPinName)
{
	FBlueprintHelperContainerActionRoleBinding Binding;
	Binding.RoleName = RoleName;
	Binding.FunctionPinName = FunctionPinName;
	Binding.bProjectToCallableRequest = true;
	return Binding;
}

FBlueprintHelperContainerActionRoleBinding UGraphWriteActionEvidenceUtils::BindOutputRole(
	const TCHAR* RoleName,
	const TCHAR* FunctionPinName)
{
	FBlueprintHelperContainerActionRoleBinding Binding;
	Binding.RoleName = RoleName;
	Binding.FunctionPinName = FunctionPinName;
	Binding.bProjectToCallableRequest = false;
	return Binding;
}

FBlueprintHelperContainerActionWildcardPolicy UGraphWriteActionEvidenceUtils::MakeWildcardPolicy(
	TArray<FString> TypedRoles)
{
	FBlueprintHelperContainerActionWildcardPolicy Policy;
	Policy.TypedRoles = MoveTemp(TypedRoles);
	return Policy;
}

FBlueprintHelperContainerActionSpec UGraphWriteActionEvidenceUtils::MakeContainerActionSpec(
	const TCHAR* OperationId,
	const TCHAR* ContainerKind,
	const TCHAR* ContainerOperation,
	const TCHAR* StableUFunctionPath,
	TArray<FString> RequiredRoles,
	TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings,
	const EBlueprintHelperContainerActionResultKind ResultKind,
	FBlueprintHelperContainerActionWildcardPolicy WildcardPolicy,
	TArray<FString> ReadbackPinRoles,
	const bool bMutatesTarget,
	const bool bReturnsValue)
{
	FBlueprintHelperContainerActionSpec Spec;
	Spec.OperationId = OperationId;
	Spec.ContainerKind = ContainerKind;
	Spec.ContainerOperation = ContainerOperation;
	Spec.StableUFunctionPath = StableUFunctionPath;
	Spec.FunctionQuery = StableUFunctionPath;
	Spec.RequiredRoles = MoveTemp(RequiredRoles);
	Spec.RoleBindings = MoveTemp(RoleBindings);
	Spec.ResultKind = ResultKind;
	Spec.WildcardPolicy = MoveTemp(WildcardPolicy);
	Spec.ReadbackPinRoles = MoveTemp(ReadbackPinRoles);
	Spec.bMutatesTarget = bMutatesTarget;
	Spec.bReturnsValue = bReturnsValue;
	Spec.bPureQuery = bReturnsValue && !bMutatesTarget;
	return Spec;
}

const TArray<FBlueprintHelperContainerActionSpec>& UGraphWriteActionEvidenceUtils::GetContainerActionSpecs()
{
	static const TArray<FBlueprintHelperContainerActionSpec> Items = []()
	{
		TArray<FBlueprintHelperContainerActionSpec> Result;
		Result.Reserve(58);

		Result.Add(MakeContainerActionSpec(TEXT("container.array.get"), TEXT("array"), TEXT("get"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Get"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("result"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.set"), TEXT("array"), TEXT("set"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Set"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("Index")), BindInputRole(TEXT("item"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.add"), TEXT("array"), TEXT("add"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.add_unique"), TEXT("array"), TEXT("add_unique"), TEXT("/Script/Engine.KismetArrayLibrary:Array_AddUnique"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.append"), TEXT("array"), TEXT("append"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Append"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("items"), TEXT("SourceArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.insert"), TEXT("array"), TEXT("insert"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Insert"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindInputRole(TEXT("index"), TEXT("Index")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.remove_item"), TEXT("array"), TEXT("remove_item"), TEXT("/Script/Engine.KismetArrayLibrary:Array_RemoveItem"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("Item")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.remove_index"), TEXT("array"), TEXT("remove_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Remove"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("IndexToRemove")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.clear"), TEXT("array"), TEXT("clear"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.contains"), TEXT("array"), TEXT("contains"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.find"), TEXT("array"), TEXT("find"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Find"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.length"), TEXT("array"), TEXT("length"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.shuffle"), TEXT("array"), TEXT("shuffle"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Shuffle"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.shuffle_from_stream"), TEXT("array"), TEXT("shuffle_from_stream"), TEXT("/Script/Engine.KismetArrayLibrary:Array_ShuffleFromStream"),
			{ TEXT("target"), TEXT("random_stream") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("random_stream"), TEXT("RandomStream")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("random_stream") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.identical"), TEXT("array"), TEXT("identical"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("ArrayA")), BindInputRole(TEXT("items"), TEXT("ArrayB")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.resize"), TEXT("array"), TEXT("resize"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Resize"),
			{ TEXT("target"), TEXT("size") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("size"), TEXT("Size")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("size") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.reverse"), TEXT("array"), TEXT("reverse"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Reverse"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.is_empty"), TEXT("array"), TEXT("is_empty"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.is_not_empty"), TEXT("array"), TEXT("is_not_empty"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.last_index"), TEXT("array"), TEXT("last_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_LastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.swap"), TEXT("array"), TEXT("swap"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Swap"),
			{ TEXT("target"), TEXT("first_index"), TEXT("second_index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("first_index"), TEXT("FirstIndex")), BindInputRole(TEXT("second_index"), TEXT("SecondIndex")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("first_index"), TEXT("second_index") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.filter_array"), TEXT("array"), TEXT("filter_array"), TEXT("/Script/Engine.KismetArrayLibrary:FilterArray"),
			{ TEXT("target"), TEXT("filter_class") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("filter_class"), TEXT("FilterClass")), BindOutputRole(TEXT("result"), TEXT("FilteredArray")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("filter_class"), TEXT("result") },
			false, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.is_valid_index"), TEXT("array"), TEXT("is_valid_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsValidIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("IndexToTest")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.random"), TEXT("array"), TEXT("random"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Random"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("OutItem")), BindOutputRole(TEXT("index"), TEXT("OutIndex")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result"), TEXT("index") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.random_from_stream"), TEXT("array"), TEXT("random_from_stream"), TEXT("/Script/Engine.KismetArrayLibrary:Array_RandomFromStream"),
			{ TEXT("target"), TEXT("random_stream") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("random_stream"), TEXT("RandomStream")), BindOutputRole(TEXT("result"), TEXT("OutItem")), BindOutputRole(TEXT("index"), TEXT("OutIndex")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("random_stream"), TEXT("result"), TEXT("index") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_string"), TEXT("array"), TEXT("sort_string"), TEXT("/Script/Engine.KismetArrayLibrary:SortStringArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_name"), TEXT("array"), TEXT("sort_name"), TEXT("/Script/Engine.KismetArrayLibrary:SortNameArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_byte"), TEXT("array"), TEXT("sort_byte"), TEXT("/Script/Engine.KismetArrayLibrary:SortByteArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_int"), TEXT("array"), TEXT("sort_int"), TEXT("/Script/Engine.KismetArrayLibrary:SortIntArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_int64"), TEXT("array"), TEXT("sort_int64"), TEXT("/Script/Engine.KismetArrayLibrary:SortInt64Array"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.array.sort_float"), TEXT("array"), TEXT("sort_float"), TEXT("/Script/Engine.KismetArrayLibrary:SortFloatArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));

		Result.Add(MakeContainerActionSpec(TEXT("container.map.add"), TEXT("map"), TEXT("add"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Add"),
			{ TEXT("target"), TEXT("key"), TEXT("value") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindInputRole(TEXT("value"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key"), TEXT("value") }),
			{ TEXT("target"), TEXT("key"), TEXT("value") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.remove"), TEXT("map"), TEXT("remove"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Remove"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			true, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.find"), TEXT("map"), TEXT("find"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Find"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.contains"), TEXT("map"), TEXT("contains"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Contains"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.keys"), TEXT("map"), TEXT("keys"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Keys"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("Keys")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.values"), TEXT("map"), TEXT("values"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Values"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("Values")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.clear"), TEXT("map"), TEXT("clear"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.length"), TEXT("map"), TEXT("length"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.is_empty"), TEXT("map"), TEXT("is_empty"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.is_not_empty"), TEXT("map"), TEXT("is_not_empty"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.get_key_value_by_index"), TEXT("map"), TEXT("get_key_value_by_index"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_GetKeyValueByIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("value"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("key"), TEXT("value") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.map.get_last_index"), TEXT("map"), TEXT("get_last_index"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_GetLastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));

		Result.Add(MakeContainerActionSpec(TEXT("container.set.add"), TEXT("set"), TEXT("add"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("NewItem")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.remove"), TEXT("set"), TEXT("remove"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Remove"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("Item")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.contains"), TEXT("set"), TEXT("contains"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.clear"), TEXT("set"), TEXT("clear"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.length"), TEXT("set"), TEXT("length"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.to_array"), TEXT("set"), TEXT("to_array"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_ToArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.add_items"), TEXT("set"), TEXT("add_items"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_AddItems"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("items"), TEXT("NewItems")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.remove_items"), TEXT("set"), TEXT("remove_items"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_RemoveItems"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("items"), TEXT("Items")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.is_empty"), TEXT("set"), TEXT("is_empty"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.is_not_empty"), TEXT("set"), TEXT("is_not_empty"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.intersection"), TEXT("set"), TEXT("intersection"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Intersection"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.union"), TEXT("set"), TEXT("union"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Union"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.difference"), TEXT("set"), TEXT("difference"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Difference"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.get_item_by_index"), TEXT("set"), TEXT("get_item_by_index"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_GetItemByIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("result"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeContainerActionSpec(TEXT("container.set.get_last_index"), TEXT("set"), TEXT("get_last_index"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_GetLastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));

		return Result;
	}();
	return Items;
}
