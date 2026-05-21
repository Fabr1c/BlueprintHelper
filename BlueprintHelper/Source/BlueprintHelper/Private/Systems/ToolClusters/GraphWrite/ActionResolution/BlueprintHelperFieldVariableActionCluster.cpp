#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsIntent(Request.Intent)
		? MakeUnsupportedClusterMigrationResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperFieldVariableActionCluster::OwnsIntent(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Get:
	case EBlueprintHelperActionIntent::Set:
	case EBlueprintHelperActionIntent::GetProperty:
	case EBlueprintHelperActionIntent::SetProperty:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = TEXT("unsupported_field_variable_cluster_intent");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster does not own intent '%s'."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = TEXT("field_variable_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster owns intent '%s', but field/variable action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::IntentToString(Request.Intent));
	return Result;
}