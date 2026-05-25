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

static FString RoleType(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
	if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
	{
		return *Type;
	}

	const FString NormalizedRole = NormalizeRole(Role);
	if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items"))
	{
		if (const FString* ElementType = Semantic.ArgumentTypes.Find(TEXT("element")))
		{
			return *ElementType;
		}
		return Semantic.ElementType;
	}
	if (NormalizedRole == TEXT("key"))
	{
		return Semantic.KeyType;
	}
	if (NormalizedRole == TEXT("value"))
	{
		return Semantic.ValueType;
	}
	return FString();
}

static FBlueprintHelperCallFunctionPinType RolePinType(
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	const FString& Role)
{
	if (const FBlueprintHelperCallFunctionPinType* PinType = Semantic.ArgumentPinTypes.Find(Role))
	{
		return *PinType;
	}

	const FString NormalizedRole = NormalizeRole(Role);
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
		if (Binding.FunctionPinName.IsEmpty() || !HasRoleEvidence(SourceSemantic, Binding.RoleName))
		{
			continue;
		}

		InOutFunctionSemantic.ArgumentNames.AddUnique(Binding.FunctionPinName);
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

	FBlueprintHelperActionResolutionRequest FunctionRequest = Request;
	FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
	FunctionRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
	FunctionRequest.Semantic.FunctionOperation = TEXT("container_action");
	FunctionRequest.Semantic.Query = Spec->FunctionQuery;
	if (FunctionRequest.Semantic.SearchMode.IsEmpty())
	{
		FunctionRequest.Semantic.SearchMode = TEXT("exact");
	}
	ProjectRoleConstraintsToFunctionPins(*Spec, Request.Semantic, FunctionRequest.Semantic);
	FunctionRequest.Semantic.TargetPath.Reset();
	FunctionRequest.Semantic.TargetObjectType.Reset();
	FunctionRequest.Semantic.TargetObjectPinType = FBlueprintHelperCallFunctionPinType();
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation_id"), Spec->OperationId);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_kind"), Spec->ContainerKind);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation"), Spec->ContainerOperation);

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
