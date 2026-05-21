#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
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
	CallRequest.Context.Blueprint = Request.Blueprint;
	CallRequest.Context.Graph = Request.TargetGraph;
	CallRequest.Context.Schema = Request.TargetGraph ? Request.TargetGraph->GetSchema() : nullptr;
	CallRequest.Context.SelfClass = Request.Blueprint
		? (Request.Blueprint->GeneratedClass ? Request.Blueprint->GeneratedClass.Get() : Request.Blueprint->SkeletonGeneratedClass.Get())
		: nullptr;
	CallRequest.Context.GraphKind = Request.TargetGraph && Request.TargetGraph->GetClass() ? Request.TargetGraph->GetClass()->GetName() : FString();
	CallRequest.Context.ArgumentNames = Request.ArgumentNames;
	CallRequest.Context.ArgumentTypes = Request.ArgumentTypes;
	CallRequest.Context.ArgumentPinTypes = Request.ArgumentPinTypes;
	CallRequest.Context.TargetObjectType = Request.TargetObjectType;
	CallRequest.Context.TargetObjectPinType = Request.TargetObjectPinType;
	CallRequest.Context.ExpectedReturnType = Request.ExpectedReturnType;
	CallRequest.Context.ExpectedReturnPinType = Request.ExpectedReturnPinType;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	if (Request.Intent == EBlueprintHelperActionIntent::Op)
	{
		return MakeUnsupportedClusterMigrationResult(Request);
	}

	if (Request.Intent != EBlueprintHelperActionIntent::Call)
	{
		return MakeUnsupportedIntentResult(Request);
	}

	FBlueprintHelperCallFunctionResolveRequest CallRequest;
	CallRequest.Blueprint = Request.Blueprint;
	CallRequest.Graph = Request.TargetGraph;
	CallRequest.Query = Request.Query;
	CallRequest.SearchMode = Request.SearchMode;
	CallRequest.AmbiguityPolicy = Request.AmbiguityPolicy;
	CallRequest.CategoryPriority = Request.CategoryPriority;
	CallRequest.ArgumentNames = Request.ArgumentNames;
	CallRequest.ArgumentTypes = Request.ArgumentTypes;
	CallRequest.ArgumentPinTypes = Request.ArgumentPinTypes;
	CallRequest.TargetObjectType = Request.TargetObjectType;
	CallRequest.TargetObjectPinType = Request.TargetObjectPinType;
	CallRequest.ExpectedReturnType = Request.ExpectedReturnType;
	CallRequest.ExpectedReturnPinType = Request.ExpectedReturnPinType;
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
	Result.ErrorCode = TEXT("unsupported_function_cluster_intent");
	Result.Message = FString::Printf(
		TEXT("FunctionActionCluster does not own intent '%s'."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFunctionActionCluster::MakeUnsupportedClusterMigrationResult(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = TEXT("operator_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("FunctionActionCluster owns intent '%s', but operator action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}