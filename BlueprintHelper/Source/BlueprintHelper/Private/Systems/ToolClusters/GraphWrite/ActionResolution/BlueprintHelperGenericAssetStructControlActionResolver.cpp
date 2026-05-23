#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

namespace
{
static bool IsGenericNodeSpawnerSemantic(const EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Select
		|| Kind == EBlueprintHelperActionSemanticKind::Control;
}

static FBlueprintHelperActionResolutionResult MakeUnsupportedGenericSemanticResult(
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

static FBlueprintHelperActionResolutionResult ResolveSingletonControlFlowNodeSpawner(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperSingletonControlFlowEvidence Evidence;
	if (FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(Request, Evidence))
	{
		return FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(Request, Evidence);
	}

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_singleton_control_flow_semantic");
	Result.Message = FString::Printf(
		TEXT("Unsupported singleton control-flow semantic '%s' with query '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.Query);
	return Result;
}
} // namespace

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!IsGenericNodeSpawnerSemantic(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedGenericSemanticResult(
			Request,
			TEXT("Generic NodeSpawner candidate resolver only accepts select/control semantics; Struct/TypeStructure construct/deconstruct must use FBlueprintHelperStructTypeStructureActionResolver."));
	}

	return ResolveSingletonControlFlowNodeSpawner(Request);
}
