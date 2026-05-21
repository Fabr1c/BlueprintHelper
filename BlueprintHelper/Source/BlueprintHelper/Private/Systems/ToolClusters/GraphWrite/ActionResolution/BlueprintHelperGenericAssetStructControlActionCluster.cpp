#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsIntent(Request.Intent)
		? MakeUnsupportedClusterMigrationResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperGenericAssetStructControlActionCluster::OwnsIntent(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Construct:
	case EBlueprintHelperActionIntent::Deconstruct:
	case EBlueprintHelperActionIntent::Select:
	case EBlueprintHelperActionIntent::Control:
	case EBlueprintHelperActionIntent::Create:
	case EBlueprintHelperActionIntent::Convert:
	case EBlueprintHelperActionIntent::Schedule:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_asset_struct_control_cluster_intent");
	Result.Message = FString::Printf(
		TEXT("GenericAssetStructControlActionCluster does not own intent '%s'."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("generic_asset_struct_control_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("GenericAssetStructControlActionCluster owns intent '%s', but generic asset/struct/control action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}