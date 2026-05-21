#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsSemanticKind(Request.Semantic.Kind)
		? MakeUnsupportedClusterMigrationResult(Request)
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

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("event_delegate_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster owns semantic kind '%s', but event/delegate action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}