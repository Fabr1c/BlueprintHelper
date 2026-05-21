#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"

namespace
{
static bool IsNodeSpawnerCandidateSemantic(const EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Construct
		|| Kind == EBlueprintHelperActionSemanticKind::Deconstruct;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!IsNodeSpawnerCandidateSemantic(Request.Semantic.Kind))
	{
		return MakeUnsupportedIntentResult(
			Request,
			TEXT("Generic NodeSpawner candidate resolver only accepts construct/deconstruct semantics."));
	}

	const FString TypeName = Request.Semantic.TypeName.TrimStartAndEnd();
	if (TypeName.IsEmpty())
	{
		return MakeNeedsContextResult(
			Request,
			TEXT("Semantic.TypeName is required to resolve construct/deconstruct NodeSpawner candidates."));
	}

	return MakeNotFoundResult(
		Request,
		FString::Printf(
			TEXT("No generic NodeSpawner candidate found for semantic '%s' and type '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*TypeName));
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeNeedsContextResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = Message;
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeNotFoundResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("generic_action_node_spawner_candidate_not_found");
	Result.Message = Message;
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeUnsupportedIntentResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_node_spawner_candidate_semantic");
	Result.Message = Message;
	return Result;
}
