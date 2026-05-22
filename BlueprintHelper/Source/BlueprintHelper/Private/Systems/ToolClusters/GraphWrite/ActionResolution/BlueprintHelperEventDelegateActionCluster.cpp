#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	return OwnsSemanticKind(Context.GetSemantic().Kind)
		? MakeNeedsMoreSemanticContextResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperEventDelegateActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("unsupported_event_delegate_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeNeedsMoreSemanticContextResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster needs event/delegate binding context for semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
