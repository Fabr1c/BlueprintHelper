#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h"

#include "BlueprintVariableNodeSpawner.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/MemberReference.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

bool FBlueprintHelperFieldVariableActionResolver::IsSupportedSemanticKind(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Field)
	{
		if (const FBlueprintHelperFieldCapabilitySpec* Spec =
			FBlueprintHelperFieldCapabilityRegistry::FindById(Semantic.CapabilityId))
		{
			return Spec->bFirstClassStatement;
		}
	}

	const FString FieldOperation = UGraphWriteActionResolverUtils::NormalizeFieldBoundaryToken(Semantic.FieldOperation);
	const FString FieldScope = UGraphWriteActionResolverUtils::NormalizeFieldBoundaryToken(Semantic.FieldScope);
	return Semantic.Kind == EBlueprintHelperActionSemanticKind::Field
		&& (FieldOperation == TEXT("get")
			|| FieldOperation == TEXT("set")
			|| FieldOperation == TEXT("get_property")
			|| FieldOperation == TEXT("set_property"))
		&& (FieldScope == TEXT("variable")
			|| FieldScope == TEXT("property_path")
			|| FieldScope == TEXT("component_ref")
			|| FieldScope == TEXT("field_access"));
}

bool FBlueprintHelperFieldVariableActionResolver::IsWritableFieldOperation(const FString& FieldOperation)
{
	return UGraphWriteActionResolverUtils::NormalizeFieldBoundaryToken(FieldOperation) == TEXT("set");
}

// ResolveFieldIdentity and ApplyResolvedFieldIdentityToCandidate moved to GraphWriteActionResolverUtils

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context) const
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;

	const FBlueprintHelperFieldCapabilitySpec* CapabilitySpec = UGraphWriteActionResolverUtils::ResolveFieldCapabilitySpecForRequest(Request);
	if (!Request.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty() && !CapabilitySpec)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ErrorCode = TEXT("unknown_field_capability");
		Result.Message = FString::Printf(
			TEXT("Unknown Field capability id: %s."),
			*Request.Semantic.CapabilityId);
		return Result;
	}

	if (CapabilitySpec && !CapabilitySpec->bFirstClassStatement)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ErrorCode = CapabilitySpec->RejectReason.IsEmpty()
			? FString(TEXT("unsupported_field_capability"))
			: CapabilitySpec->RejectReason;
		Result.Message = FString::Printf(
			TEXT("Field capability is not a first-class user statement: capability=%s reason=%s."),
			*CapabilitySpec->Id,
			*Result.ErrorCode);
		return Result;
	}

	if (!IsSupportedSemanticKind(Context.GetSemantic()) && !CapabilitySpec)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("needs_more_semantic_context");
		Result.Message = FString::Printf(
			TEXT("FieldVariableActionCluster needs field semantic context: semantic=%s field_operation=%s field_scope=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope);
		return Result;
	}

	FBlueprintHelperActionResolutionRequest EffectiveRequest = Request;
	if (CapabilitySpec)
	{
		if (EffectiveRequest.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.CapabilityId = CapabilitySpec->Id;
		}
		if (EffectiveRequest.Semantic.FieldOperation.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.FieldOperation = CapabilitySpec->FieldOperation;
		}
		if (EffectiveRequest.Semantic.FieldScope.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.FieldScope = CapabilitySpec->FieldScope;
		}
	}
	const TMap<FString, FString>& Evidence = Context.GetEvidence();
	UGraphWriteActionResolverUtils::BackfillCapabilityFactsFromEvidence(EffectiveRequest, Evidence);

	if (!Request.Blueprint)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("field_variable_missing_blueprint");
		Result.Message = TEXT("field_variable_missing_blueprint");
		return Result;
	}

	if (!Request.TargetGraph)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("field_variable_missing_target_graph");
		Result.Message = TEXT("field_variable_missing_target_graph");
		return Result;
	}

	UClass* OwnerClass = UGraphWriteActionResolverUtils::ResolveOwnerClass(EffectiveRequest.Blueprint);
	TSubclassOf<UK2Node_Variable> NodeClass = (CapabilitySpec
		? UGraphWriteActionResolverUtils::FieldCapabilityWrites(*CapabilitySpec)
		: IsWritableFieldOperation(EffectiveRequest.Semantic.FieldOperation))
		? UK2Node_VariableSet::StaticClass()
		: UK2Node_VariableGet::StaticClass();

	const bool bComponentRefSemantic = UGraphWriteActionResolverUtils::IsComponentRefFieldScope(EffectiveRequest.Semantic.FieldScope)
		|| (CapabilitySpec && CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentRef);
	const bool bFieldAccessSemantic = UGraphWriteActionResolverUtils::IsFieldAccessFieldScope(EffectiveRequest.Semantic.FieldScope)
		|| (CapabilitySpec
			&& (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentProperty
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::StructMember
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath));
	const bool bFunctionScopedSemantic = CapabilitySpec && UGraphWriteActionResolverUtils::IsFunctionScopedCapability(*CapabilitySpec);
	if (CapabilitySpec && CapabilitySpec->bRequiresTargetPin)
	{
		const FString TargetPinRef = UGraphWriteActionResolverUtils::CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_ref"), TEXT("target_pin_ref"));
		const FString TargetPinCategory = UGraphWriteActionResolverUtils::CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_type"), TEXT("linked_pin_type_category"));
		const FString TargetPinObjectPath = UGraphWriteActionResolverUtils::CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_object_path"), TEXT("linked_pin_type_object_path"));
		if (TargetPinRef.IsEmpty() || TargetPinCategory.IsEmpty() || TargetPinObjectPath.IsEmpty())
		{
			return UGraphWriteActionResolverUtils::MakeFieldMissingEvidenceResult(
				TEXT("Field capability requires explicit target pin projection."),
				TEXT("missing_target_pin_projection"));
		}

		UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_ref"), TargetPinRef);
		UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_type"), TargetPinCategory);
		UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_object_path"), TargetPinObjectPath);
		if (!EffectiveRequest.Semantic.TargetObjectPinType.IsValid())
		{
			EffectiveRequest.Semantic.TargetObjectPinType.Category = TargetPinCategory;
			EffectiveRequest.Semantic.TargetObjectPinType.ObjectPath = TargetPinObjectPath;
		}
	}
	const FBlueprintHelperResolvedFieldPath ResolvedPath =
		FBlueprintHelperFieldPathResolution::Resolve(EffectiveRequest, Evidence);

	if (!ResolvedPath.bIsValid)
	{
		return UGraphWriteActionResolverUtils::MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("%s semantic=%s field_operation=%s field_scope=%s query=%s target=%s property_path=%s."),
			*ResolvedPath.Message,
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath,
			*Request.Semantic.PropertyPath),
			ResolvedPath.ErrorCode.IsEmpty() ? TEXT("needs_more_semantic_context") : ResolvedPath.ErrorCode);
	}

	if (!bComponentRefSemantic && !bFieldAccessSemantic && !bFunctionScopedSemantic && ResolvedPath.OwnerClassPath.IsEmpty())
	{
		return UGraphWriteActionResolverUtils::MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("Field variable action requires explicit owner evidence: semantic=%s field_operation=%s field_scope=%s query=%s target=%s property_path=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath,
			*Request.Semantic.PropertyPath));
	}

	FString FieldName = UGraphWriteActionResolverUtils::FirstNonEmptyFieldValue(
		EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name")),
		EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_name")),
		UGraphWriteActionResolverUtils::GetEvidenceValue(Evidence, TEXT("field_name")));
	if (FieldName.IsEmpty() && bFunctionScopedSemantic)
	{
		FieldName = UGraphWriteActionResolverUtils::FirstNonEmptyFieldValue(
			EffectiveRequest.Semantic.Query,
			EffectiveRequest.Semantic.TargetPath);
	}
	if (FieldName.IsEmpty() && ResolvedPath.Role != EBlueprintHelperFieldPathRole::Variable)
	{
		FieldName = ResolvedPath.LeafName;
	}

	if (FieldName.IsEmpty() && !EffectiveRequest.Semantic.TargetPath.TrimStartAndEnd().IsEmpty())
	{
		FBlueprintHelperActionResolutionRequest CandidateRequest = EffectiveRequest;
		CandidateRequest.Semantic.Query = EffectiveRequest.Semantic.TargetPath.TrimStartAndEnd();
		const TArray<FBlueprintHelperVariableActionCandidate> Candidates =
			UGraphWriteActionResolverUtils::BuildProjectedFieldCandidates(CandidateRequest, OwnerClass, NodeClass.Get());
		if (Candidates.Num() == 1)
		{
			FieldName = Candidates[0].Info.DisplayName;
		}
		else if (Candidates.Num() > 1)
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
			Result.ErrorCode = TEXT("ambiguous_candidates");
			Result.Message = FString::Printf(
				TEXT("Field variable action is ambiguous without projected field_name evidence: semantic=%s field_operation=%s field_scope=%s owner=%s query=%s."),
				*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
				*Request.Semantic.FieldOperation,
				*Request.Semantic.FieldScope,
				*ResolvedPath.OwnerClassPath,
				*Request.Semantic.Query);
			UGraphWriteActionResolverUtils::SetFieldCandidateDiagnostics(Result, Candidates, Request.MaxCandidates);
			return Result;
		}
	}

	if (FieldName.TrimStartAndEnd().IsEmpty())
	{
		return UGraphWriteActionResolverUtils::MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("Field variable action requires projected field_name evidence: semantic=%s field_operation=%s field_scope=%s owner=%s query=%s target=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*ResolvedPath.OwnerClassPath,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath));
	}

	const FProperty* Property = nullptr;
	FProperty* MutableProperty = nullptr;
	FBPVariableDescription* LocalVariable = nullptr;
	bool bResolvedLocalVariable = false;
	bool bResolvedFunctionParam = false;

	if (bFunctionScopedSemantic)
	{
		FString FunctionScopeName;
		if (!UGraphWriteActionResolverUtils::ResolveFunctionScope(EffectiveRequest, Evidence, *CapabilitySpec, FunctionScopeName, Result))
		{
			return Result;
		}
		UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.local_scope"), FunctionScopeName);
		UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.function_name"), FunctionScopeName);

		if (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::Local)
		{
			UK2Node_FunctionEntry* EntryNode = nullptr;
			LocalVariable = UGraphWriteActionResolverUtils::FindLocalVariableDescription(EffectiveRequest, FieldName, &EntryNode);
			if (!LocalVariable || !EntryNode)
			{
				Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
				Result.ErrorCode = TEXT("local_variable_not_found");
				Result.Message = FString::Printf(
					TEXT("Local Field capability not found in function scope: field=%s function=%s."),
					*FieldName,
					*FunctionScopeName);
				return Result;
			}

			MutableProperty = UGraphWriteActionResolverUtils::ResolveLocalVariableProperty(EffectiveRequest, *LocalVariable);
			Property = MutableProperty;
			bResolvedLocalVariable = true;
		}
		else if (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::FunctionParam)
		{
			const FString ParamFlags = EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.param_flags"));
			FString ParamErrorCode;
			UFunction* Function = UGraphWriteActionResolverUtils::FindFunctionForScope(
				EffectiveRequest.Blueprint,
				EffectiveRequest.TargetGraph,
				FunctionScopeName);
			MutableProperty = UGraphWriteActionResolverUtils::FindFunctionInputParameter(Function, FieldName, ParamFlags, ParamErrorCode);
			Property = MutableProperty;
			bResolvedFunctionParam = true;
			if (Function && EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name")).IsEmpty())
			{
				UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.function_name"), Function->GetName());
			}
		}
	}
	else
	{
		Property = UGraphWriteActionResolverUtils::ResolveFieldProperty(EffectiveRequest, CapabilitySpec, FieldName);
		MutableProperty = const_cast<FProperty*>(Property);
	}

	if (!Property)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = bFunctionScopedSemantic
			? FString(TEXT("function_scope_field_unresolvable"))
			: FString(TEXT("field_action_unresolvable"));
		Result.Message = FString::Printf(
			TEXT("Field variable action not found from projected context: semantic=%s field_operation=%s field_scope=%s field=%s query=%s target=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*FieldName,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath);
		return Result;
	}

	const FObjectPropertyBase* ComponentObjectProperty = nullptr;
	if (CapabilitySpec && CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentRef)
	{
		ComponentObjectProperty = CastField<FObjectPropertyBase>(Property);
		const UClass* ComponentPropertyClass = ComponentObjectProperty
			? ComponentObjectProperty->PropertyClass
			: nullptr;
		if (!ComponentObjectProperty || !ComponentPropertyClass || !ComponentPropertyClass->IsChildOf(UActorComponent::StaticClass()))
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
			Result.ErrorCode = TEXT("not_class_component_property");
			Result.Message = FString::Printf(
				TEXT("Component ref field action requires an object component property: field=%s."),
				*FieldName);
			return Result;
		}
	}

	UBlueprintVariableNodeSpawner* Spawner = bResolvedLocalVariable && LocalVariable
		? UBlueprintVariableNodeSpawner::CreateFromLocal(NodeClass, EffectiveRequest.TargetGraph, *LocalVariable, MutableProperty)
		: UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(
			NodeClass,
			Property,
			bResolvedFunctionParam ? EffectiveRequest.TargetGraph : nullptr);
	if (!Spawner)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = TEXT("field_action_unresolvable");
		Result.Message = FString::Printf(
			TEXT("Field variable node spawner unavailable: semantic=%s field_operation=%s field_scope=%s field=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*FieldName);
		return Result;
	}

	FBlueprintHelperCallFunctionCandidateInfo CandidateInfo;
	const FString StableIdPrefix = bComponentRefSemantic
		? TEXT("field_component_ref")
		: (bFieldAccessSemantic ? TEXT("field_access") : TEXT("field_variable"));
	CandidateInfo.StableId = UGraphWriteActionResolverUtils::MakeVariableStableId(
		EffectiveRequest.Blueprint,
		FieldName,
		EffectiveRequest.Semantic.FieldOperation,
		EffectiveRequest.Semantic.FieldScope,
		StableIdPrefix);
	CandidateInfo.DisplayName = FieldName;
	CandidateInfo.OwnerClassPath = ResolvedPath.OwnerClassPath;
	CandidateInfo.NativeFunctionName = FieldName;
	CandidateInfo.Category = bComponentRefSemantic
		? TEXT("field_component_ref")
		: (bFieldAccessSemantic ? TEXT("field_access") : TEXT("field_variable"));
	CandidateInfo.NodeClassPath = NodeClass.Get() ? NodeClass->GetPathName() : FString();
	CandidateInfo.MatchReason = FString::Printf(
		TEXT("projected_context_evidence semantic=%s field_operation=%s field_scope=%s field=%s path=%s root=%s leaf=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.FieldOperation,
		*Request.Semantic.FieldScope,
		*FieldName,
		*ResolvedPath.FullPath,
		*ResolvedPath.RootName,
		*ResolvedPath.LeafName);
	CandidateInfo.ReturnType = !Evidence.FindRef(TEXT("field_pin_category")).IsEmpty()
		? Evidence.FindRef(TEXT("field_pin_category"))
		: Property->GetCPPType();
	CandidateInfo.TargetObjectPin = TEXT("self");
	CandidateInfo.Score = 100;
	CandidateInfo.bGraphCompatible = true;
	if (CapabilitySpec)
	{
		FBlueprintHelperResolvedFieldIdentity Identity;
		if (UGraphWriteActionResolverUtils::ResolveFieldIdentity(EffectiveRequest, *CapabilitySpec, FieldName, Property, OwnerClass, Identity))
		{
			UGraphWriteActionResolverUtils::ApplyResolvedFieldIdentityToCandidate(Identity, CandidateInfo);
		}
	}
	if (ComponentObjectProperty)
	{
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_name"), FieldName);
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_owner_class"), EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_owner_class")));
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_kind"), EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_kind")));
		CandidateInfo.ReadbackFacts.Add(TEXT("component_name"), FieldName);
		CandidateInfo.ReadbackFacts.Add(TEXT("component_ref_spawner"), TEXT("UBlueprintVariableNodeSpawner"));
		CandidateInfo.ReadbackFacts.Add(
			TEXT("component_property_class"),
			ComponentObjectProperty->PropertyClass ? ComponentObjectProperty->PropertyClass->GetPathName() : FString());
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.SelectedStableId = CandidateInfo.StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Reset();
	Result.CandidateActions.Add(CandidateInfo);
	Result.bRequiresDedicatedFragmentBuilder = ResolvedPath.bRequiresFragmentDecomposition;
	Result.MatchReason = ResolvedPath.bRequiresFragmentDecomposition
		? TEXT("complex_property_path_requires_field_path_fragment_builder")
		: CandidateInfo.MatchReason;
	Result.Message = FString::Printf(
		TEXT("Field variable action resolved: semantic=%s field_operation=%s field_scope=%s variable=%s."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.FieldOperation,
		*Request.Semantic.FieldScope,
		*FieldName);
	return Result;
}
