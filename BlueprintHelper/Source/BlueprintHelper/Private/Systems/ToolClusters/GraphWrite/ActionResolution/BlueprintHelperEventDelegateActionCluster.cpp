#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsIntent(Request.Intent)
		? MakeUnsupportedClusterMigrationResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperEventDelegateActionCluster::OwnsIntent(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Event:
	case EBlueprintHelperActionIntent::ComponentBoundEvent:
	case EBlueprintHelperActionIntent::Bind:
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
	Result.ErrorCode = TEXT("unsupported_event_delegate_cluster_intent");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster does not own intent '%s'."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("event_delegate_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster owns intent '%s', but event/delegate action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}