#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"

bool FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
	const FBlueprintHelperResolvedActionContextBundle& Bundle,
	const FString& StatementId,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	const FBlueprintHelperResolvedActionContext* Context = Bundle.FindByStatementId(StatementId);
	if (!Context)
	{
		OutError = FString::Printf(TEXT("action_context_not_found:%s"), *StatementId);
		return false;
	}

	if (!Blueprint || !Graph)
	{
		OutError = TEXT("action_context_missing_blueprint_or_graph");
		return false;
	}

	if (Context->ClusterKind == EBlueprintHelperSpawnerClusterKind::Unknown
		|| Context->Semantic.Kind == EBlueprintHelperActionSemanticKind::Unknown)
	{
		OutError = FString::Printf(TEXT("action_context_unresolved_semantic:%s"), *StatementId);
		return false;
	}

	OutRequest = FBlueprintHelperActionResolutionRequest();
	OutRequest.ClusterKind = Context->ClusterKind;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = Graph;
	OutRequest.Semantic = Context->Semantic;
	return true;
}
