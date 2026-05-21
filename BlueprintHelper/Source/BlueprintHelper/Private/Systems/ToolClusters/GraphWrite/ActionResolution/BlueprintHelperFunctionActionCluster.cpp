#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

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

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FBlueprintHelperActionSemanticConstraints& Semantic = Request.Semantic;
	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Op)
	{
		return FBlueprintHelperOperatorActionResolver::Resolve(Request);
	}

	if (Semantic.Kind != EBlueprintHelperActionSemanticKind::Call)
	{
		return MakeUnsupportedIntentResult(Request);
	}

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

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionActionCluster::MakeUnsupportedIntentResult(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = TEXT("unsupported_function_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("FunctionActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
