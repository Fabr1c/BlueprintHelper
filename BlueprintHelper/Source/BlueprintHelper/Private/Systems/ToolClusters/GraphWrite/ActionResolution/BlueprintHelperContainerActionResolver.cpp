#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"

namespace
{
static FBlueprintHelperActionResolutionResult MakeInvalid(const FString& Code, const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = Code;
	Result.Message = Message;
	return Result;
}

static FString NormalizeRole(const FString& Role)
{
	return Role.TrimStartAndEnd().ToLower();
}

static FString ExtractStableFunctionName(const FString& StableFunctionPath)
{
	int32 ColonIndex = INDEX_NONE;
	if (StableFunctionPath.FindLastChar(TEXT(':'), ColonIndex)
		&& ColonIndex > 0
		&& ColonIndex < StableFunctionPath.Len() - 1)
	{
		return StableFunctionPath.Mid(ColonIndex + 1).TrimStartAndEnd();
	}
	return StableFunctionPath.TrimStartAndEnd();
}

static FString ResultKindToString(const EBlueprintHelperContainerActionResultKind ResultKind)
{
	switch (ResultKind)
	{
	case EBlueprintHelperContainerActionResultKind::ReturnValue:
		return TEXT("return_value");
	case EBlueprintHelperContainerActionResultKind::OutputPins:
		return TEXT("output_pins");
	case EBlueprintHelperContainerActionResultKind::None:
	default:
		return TEXT("none");
	}
}

static FString ContainerActionPermittedNodeClassPaths()
{
	return TEXT("/Script/BlueprintGraph.K2Node_CallFunction;/Script/BlueprintGraph.K2Node_CallArrayFunction");
}

static bool HasArgumentName(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	return Semantic.ArgumentNames.ContainsByPredicate(
		[&Role](const FString& Candidate)
		{
			return Candidate.Equals(Role, ESearchCase::IgnoreCase);
		});
}

static bool HasRoleTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	if (Semantic.ArgumentTypes.Contains(Role) || Semantic.ArgumentPinTypes.Contains(Role))
	{
		return true;
	}

	return false;
}

static bool HasRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	const FString NormalizedRole = NormalizeRole(Role);
	if (NormalizedRole == TEXT("target"))
	{
		return !Semantic.TargetPath.IsEmpty()
			|| !Semantic.TargetObjectType.IsEmpty()
			|| Semantic.TargetObjectPinType.IsValid()
			|| HasArgumentName(Semantic, Role)
			|| Semantic.DefaultValues.Contains(Role);
	}

	return HasArgumentName(Semantic, Role)
		|| Semantic.DefaultValues.Contains(Role)
		|| HasRoleTypeEvidence(Semantic, Role);
}

static bool HasContainerTargetTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	const FString Kind = NormalizeRole(Semantic.ContainerKind);
	if (Kind == TEXT("map"))
	{
		return (!Semantic.KeyType.IsEmpty() && !Semantic.ValueType.IsEmpty())
			|| (Semantic.ContainerKeyPinType.IsValid() && Semantic.ContainerValuePinType.IsValid())
			|| Semantic.ArgumentPinTypes.Contains(TEXT("target"));
	}

	return !Semantic.ElementType.IsEmpty()
		|| Semantic.ContainerElementPinType.IsValid()
		|| Semantic.ArgumentPinTypes.Contains(TEXT("target"));
}

static bool HasTypedRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	const FString NormalizedRole = NormalizeRole(Role);
	if (NormalizedRole == TEXT("target"))
	{
		return HasContainerTargetTypeEvidence(Semantic);
	}
	if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items") || NormalizedRole == TEXT("other") || NormalizedRole == TEXT("result"))
	{
		return !Semantic.ElementType.IsEmpty()
			|| Semantic.ContainerElementPinType.IsValid()
			|| Semantic.ArgumentTypes.Contains(TEXT("element"))
			|| Semantic.ArgumentPinTypes.Contains(TEXT("element"))
			|| Semantic.ArgumentTypes.Contains(Role)
			|| Semantic.ArgumentPinTypes.Contains(Role);
	}
	if (NormalizedRole == TEXT("key"))
	{
		return !Semantic.KeyType.IsEmpty()
			|| Semantic.ContainerKeyPinType.IsValid()
			|| Semantic.ArgumentTypes.Contains(Role)
			|| Semantic.ArgumentPinTypes.Contains(Role);
	}
	if (NormalizedRole == TEXT("value"))
	{
		return !Semantic.ValueType.IsEmpty()
			|| Semantic.ContainerValuePinType.IsValid()
			|| Semantic.ArgumentTypes.Contains(Role)
			|| Semantic.ArgumentPinTypes.Contains(Role);
	}

	return true;
}

static bool TryValidateRequiredRoles(
	const FBlueprintHelperContainerActionSpec& Spec,
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	FString& OutMissingRole)
{
	OutMissingRole.Reset();
	for (const FString& RequiredRole : Spec.RequiredRoles)
	{
		if (!HasRoleEvidence(Semantic, RequiredRole))
		{
			OutMissingRole = RequiredRole;
			return false;
		}
	}
	return true;
}

static bool TryValidateTypedRoleEvidence(
	const FBlueprintHelperContainerActionSpec& Spec,
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	FString& OutMissingRole)
{
	OutMissingRole.Reset();
	for (const FString& TypedRole : Spec.WildcardPolicy.TypedRoles)
	{
		if (!HasTypedRoleEvidence(Semantic, TypedRole))
		{
			OutMissingRole = TypedRole;
			return false;
		}
	}
	return true;
}

static FString RoleType(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	const FString NormalizedRole = NormalizeRole(Role);
	if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items") || NormalizedRole == TEXT("other") || NormalizedRole == TEXT("result"))
	{
		if (!Semantic.ElementType.IsEmpty())
		{
			return Semantic.ElementType;
		}
		if (const FString* ElementType = Semantic.ArgumentTypes.Find(TEXT("element")))
		{
			return *ElementType;
		}
		if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
		{
			return *Type;
		}
		return FString();
	}
	if (NormalizedRole == TEXT("key"))
	{
		if (!Semantic.KeyType.IsEmpty())
		{
			return Semantic.KeyType;
		}
		if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
		{
			return *Type;
		}
		return FString();
	}
	if (NormalizedRole == TEXT("value"))
	{
		if (!Semantic.ValueType.IsEmpty())
		{
			return Semantic.ValueType;
		}
		if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
		{
			return *Type;
		}
		return FString();
	}
	if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
	{
		return *Type;
	}
	return FString();
}

static FBlueprintHelperCallFunctionPinType RolePinType(
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	const FString& Role)
{
	const FString NormalizedRole = NormalizeRole(Role);
	if (NormalizedRole == TEXT("target"))
	{
		return FBlueprintHelperCallFunctionPinType();
	}

	if (const FBlueprintHelperCallFunctionPinType* PinType = Semantic.ArgumentPinTypes.Find(Role))
	{
		return *PinType;
	}
	if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items"))
	{
		if (const FBlueprintHelperCallFunctionPinType* ElementPinType = Semantic.ArgumentPinTypes.Find(TEXT("element")))
		{
			return *ElementPinType;
		}
		return Semantic.ContainerElementPinType;
	}
	if (NormalizedRole == TEXT("key"))
	{
		return Semantic.ContainerKeyPinType;
	}
	if (NormalizedRole == TEXT("value"))
	{
		return Semantic.ContainerValuePinType;
	}
	return FBlueprintHelperCallFunctionPinType();
}

static bool IsWildcardTypedRole(const FBlueprintHelperContainerActionSpec& Spec, const FString& Role)
{
	return Spec.WildcardPolicy.TypedRoles.ContainsByPredicate(
		[&Role](const FString& TypedRole)
		{
			return NormalizeRole(TypedRole).Equals(NormalizeRole(Role), ESearchCase::IgnoreCase);
		});
}

static void ProjectRoleConstraintsToFunctionPins(
	const FBlueprintHelperContainerActionSpec& Spec,
	const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
	FBlueprintHelperActionSemanticConstraints& InOutFunctionSemantic)
{
	InOutFunctionSemantic.ArgumentNames.Reset();
	InOutFunctionSemantic.ArgumentTypes.Reset();
	InOutFunctionSemantic.ArgumentPinTypes.Reset();

	for (const FBlueprintHelperContainerActionRoleBinding& Binding : Spec.RoleBindings)
	{
		if (!Binding.bProjectToCallableRequest
			|| Binding.FunctionPinName.IsEmpty()
			|| !HasRoleEvidence(SourceSemantic, Binding.RoleName))
		{
			continue;
		}

		InOutFunctionSemantic.ArgumentNames.AddUnique(Binding.FunctionPinName);
		if (IsWildcardTypedRole(Spec, Binding.RoleName))
		{
			continue;
		}

		const FString Type = RoleType(SourceSemantic, Binding.RoleName);
		if (!Type.IsEmpty())
		{
			InOutFunctionSemantic.ArgumentTypes.Add(Binding.FunctionPinName, Type);
		}
		const FBlueprintHelperCallFunctionPinType PinType = RolePinType(SourceSemantic, Binding.RoleName);
		if (PinType.IsValid())
		{
			InOutFunctionSemantic.ArgumentPinTypes.Add(Binding.FunctionPinName, PinType);
		}
	}
}

static FBlueprintHelperActionResolutionResult ResolveInternal(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Request.Semantic.ContainerKind, Request.Semantic.ContainerOperation);
	if (!Spec)
	{
		return MakeInvalid(TEXT("unsupported_container_operation"), TEXT("Unsupported container_action operation."));
	}

	FString MissingRole;
	if (!TryValidateRequiredRoles(*Spec, Request.Semantic, MissingRole))
	{
		return MakeInvalid(
			TEXT("container_action_role_missing"),
			FString::Printf(TEXT("container_action %s requires role '%s'."), *Spec->OperationId, *MissingRole));
	}
	if (!TryValidateTypedRoleEvidence(*Spec, Request.Semantic, MissingRole))
	{
		return MakeInvalid(
			TEXT("container_action_type_evidence_missing"),
			FString::Printf(TEXT("container_action %s requires typed callable evidence for role '%s'."), *Spec->OperationId, *MissingRole));
	}

	FBlueprintHelperActionResolutionRequest FunctionRequest = Request;
	FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
	FunctionRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
	FunctionRequest.Semantic.FunctionOperation = TEXT("container_action");
	FunctionRequest.Semantic.Query = ExtractStableFunctionName(Spec->StableUFunctionPath);
	FunctionRequest.Semantic.StableId = Spec->StableUFunctionPath;
	FunctionRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("container.stable_ufunction_path")) = Spec->StableUFunctionPath;
	FunctionRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("function.permitted_node_class_paths")) = ContainerActionPermittedNodeClassPaths();
	if (FunctionRequest.Semantic.SearchMode.IsEmpty())
	{
		FunctionRequest.Semantic.SearchMode = TEXT("exact");
	}
	FunctionRequest.Semantic.DefaultValues.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);
	FunctionRequest.Semantic.DefaultValues.Add(TEXT("function.permitted_node_class_paths"), ContainerActionPermittedNodeClassPaths());
	ProjectRoleConstraintsToFunctionPins(*Spec, Request.Semantic, FunctionRequest.Semantic);
	FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Array"));
	FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Map"));
	FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Set"));
	FunctionRequest.Semantic.TargetPath.Reset();
	FunctionRequest.Semantic.TargetObjectType.Reset();
	FunctionRequest.Semantic.TargetObjectPinType = FBlueprintHelperCallFunctionPinType();
	FunctionRequest.Semantic.DefaultValues.Add(TEXT("container.result_kind"), ResultKindToString(Spec->ResultKind));
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation_id"), Spec->OperationId);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_kind"), Spec->ContainerKind);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation"), Spec->ContainerOperation);
	FunctionRequest.ContextEvidence.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);

	const FBlueprintHelperActionClusterContextView FunctionContext(FunctionRequest);
	FString ContextErrorCode;
	FString ContextMessage;
	if (!FunctionContext.IsCompleteForCluster(EBlueprintHelperSpawnerClusterKind::FunctionAction, ContextErrorCode, ContextMessage))
	{
		FBlueprintHelperActionResolutionResult Result = MakeInvalid(ContextErrorCode, ContextMessage);
		Result.MatchReason = Spec->OperationId;
		return Result;
	}

	FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperFunctionActionCluster::Resolve(FunctionRequest, FunctionContext);
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	if (Result.IsResolved())
	{
		Result.MatchReason = Spec->OperationId + TEXT(" via ") + Result.MatchReason;
	}
	return Result;
}
} // namespace

FBlueprintHelperActionResolutionResult FBlueprintHelperContainerActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	return ResolveInternal(Request);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperContainerActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	return ResolveInternal(Context.GetRequest().StatementId == Request.StatementId ? Request : Context.GetRequest());
}
