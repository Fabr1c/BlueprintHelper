#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"

namespace
{
static EBlueprintHelperActionResolutionStatus MapFunctionResolveStatus(EBlueprintHelperCallFunctionResolveStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperCallFunctionResolveStatus::Resolved:
		return EBlueprintHelperActionResolutionStatus::Resolved;
	case EBlueprintHelperCallFunctionResolveStatus::Ambiguous:
		return EBlueprintHelperActionResolutionStatus::Ambiguous;
	case EBlueprintHelperCallFunctionResolveStatus::Blocked:
		return EBlueprintHelperActionResolutionStatus::Blocked;
	case EBlueprintHelperCallFunctionResolveStatus::NotFound:
	default:
		return EBlueprintHelperActionResolutionStatus::NotFound;
	}
}

static FString GetDefaultValue(
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	const TCHAR* Key)
{
	return Semantic.DefaultValues.FindRef(Key).TrimStartAndEnd();
}

static FString GetFunctionOperation(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	const FString SemanticOperation = Semantic.FunctionOperation.TrimStartAndEnd();
	return SemanticOperation.IsEmpty()
		? GetDefaultValue(Semantic, TEXT("function_operation"))
		: SemanticOperation;
}

static bool HasTypedArgumentPinEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	for (const TPair<FString, FBlueprintHelperCallFunctionPinType>& Pair : Semantic.ArgumentPinTypes)
	{
		if (Pair.Value.IsValid())
		{
			return true;
		}
	}
	return false;
}

static bool IsTrueEvidence(const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
	const FString Value = Evidence.FindRef(Key).TrimStartAndEnd();
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value == TEXT("1")
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}

static void PopulateCallContext(FBlueprintHelperCallFunctionResolveRequest& CallRequest, const FBlueprintHelperActionResolutionRequest& Request)
{
	const FBlueprintHelperActionSemanticConstraints& Semantic = Request.Semantic;
	CallRequest.Context.Blueprint = Request.Blueprint;
	CallRequest.Context.Graph = Request.TargetGraph;
	CallRequest.Context.Schema = Request.TargetGraph ? Request.TargetGraph->GetSchema() : nullptr;
	CallRequest.Context.SelfClass = Request.Blueprint
		? (Request.Blueprint->GeneratedClass ? Request.Blueprint->GeneratedClass.Get() : Request.Blueprint->SkeletonGeneratedClass.Get())
		: nullptr;
	CallRequest.Context.GraphKind = Request.TargetGraph && Request.TargetGraph->GetClass() ? Request.TargetGraph->GetClass()->GetName() : FString();
	CallRequest.Context.ArgumentNames = Semantic.ArgumentNames;
	CallRequest.Context.ArgumentTypes = Semantic.ArgumentTypes;
	CallRequest.Context.ArgumentPinTypes = Semantic.ArgumentPinTypes;
	CallRequest.Context.TargetObjectType = Semantic.TargetObjectType;
	CallRequest.Context.TargetObjectPinType = Semantic.TargetObjectPinType;
	CallRequest.Context.ExpectedReturnType = Semantic.ExpectedReturnType;
	CallRequest.Context.ExpectedReturnPinType = Semantic.ExpectedReturnPinType;
}

static FBlueprintHelperActionResolutionResult MakeInvalidRequestResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult ResolveViaCallFunctionResolver(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	FBlueprintHelperCallFunctionResolveRequest CallRequest;
	CallRequest.Blueprint = Request.Blueprint;
	CallRequest.Graph = Request.TargetGraph;
	CallRequest.Query = Semantic.Query;
	CallRequest.SearchMode = Semantic.SearchMode;
	CallRequest.AmbiguityPolicy = Semantic.AmbiguityPolicy;
	CallRequest.CategoryPriority = Semantic.CategoryPriority;
	CallRequest.ArgumentNames = Semantic.ArgumentNames;
	CallRequest.ArgumentTypes = Semantic.ArgumentTypes;
	CallRequest.ArgumentPinTypes = Semantic.ArgumentPinTypes;
	CallRequest.TargetObjectType = Semantic.TargetObjectType;
	CallRequest.TargetObjectPinType = Semantic.TargetObjectPinType;
	CallRequest.ExpectedReturnType = Semantic.ExpectedReturnType;
	CallRequest.ExpectedReturnPinType = Semantic.ExpectedReturnPinType;
	CallRequest.bAllowFuzzyUnique = Request.bAllowFuzzyUnique;
	CallRequest.MaxCandidates = Request.MaxCandidates;
	if (!Semantic.StableId.TrimStartAndEnd().IsEmpty())
	{
		CallRequest.CandidatePolicy.RequiredStableCallableIds.Add(Semantic.StableId.TrimStartAndEnd());
	}
	PopulateCallContext(CallRequest, Request);

	const FBlueprintHelperCallFunctionResolveResult CallResult = FBlueprintHelperCallFunctionResolver::Resolve(CallRequest);

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = MapFunctionResolveStatus(CallResult.Status);
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = CallResult.ErrorCode;
	Result.Message = CallResult.Message;
	Result.SelectedStableId = CallResult.Selected.StableId;
	Result.SelectedSpawner = CallResult.Selected.NodeSpawner;
	Result.SelectedFunction = CallResult.Selected.Function;
	Result.CandidateActions = CallResult.CandidateFunctions;
	Result.FunctionCandidate = CallResult.Selected;
	return Result;
}
}

bool FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(
	const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	const FString FunctionOperation = NormalizeOperation(GetFunctionOperation(Semantic));
	switch (Semantic.Kind)
	{
	case EBlueprintHelperActionSemanticKind::Create:
		return FunctionOperation == TEXT("create_function");
	case EBlueprintHelperActionSemanticKind::Convert:
		return FunctionOperation == TEXT("convert_function");
	case EBlueprintHelperActionSemanticKind::Schedule:
		return FunctionOperation == TEXT("schedule_function")
			|| FunctionOperation == TEXT("latent_or_async_function");
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionSemanticActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperActionSemanticConstraints& Semantic = Context.GetSemantic();
	const FString Query = Semantic.Query.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		return MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Function semantic callable resolution requires a non-empty query."));
	}

	const FString FunctionOperation = NormalizeOperation(GetFunctionOperation(Semantic));
	if (!IsSupportedSemanticKind(Semantic))
	{
		return MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			FString::Printf(
				TEXT("Unsupported function semantic operation '%s' for semantic kind '%s'."),
				FunctionOperation.IsEmpty() ? TEXT("<empty>") : *FunctionOperation,
				*FBlueprintHelperActionResolutionCore::SemanticKindToString(Semantic.Kind)));
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Convert
		&& !HasTypedArgumentPinEvidence(Semantic)
		&& !Semantic.ExpectedReturnPinType.IsValid())
	{
		return MakeInvalidRequestResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Convert callable resolution requires typed argument pins or expected return pin evidence."));
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Schedule
		&& FunctionOperation == TEXT("latent_or_async_function")
		&& !IsTrueEvidence(Request.ContextEvidence, TEXT("graph_latent_allowed")))
	{
		return MakeInvalidRequestResult(
			TEXT("latent_function_not_allowed_in_graph"),
			TEXT("Latent or async function scheduling requires graph_latent_allowed=true evidence."));
	}

	return ResolveViaCallFunctionResolver(Request, Semantic);
}
