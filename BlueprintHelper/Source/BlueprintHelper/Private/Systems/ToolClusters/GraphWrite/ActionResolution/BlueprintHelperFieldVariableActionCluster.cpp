#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsSemanticKind(Request.Semantic.Kind)
		? MakeUnsupportedClusterMigrationResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperFieldVariableActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
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
	Result.ErrorCode = TEXT("unsupported_field_variable_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = TEXT("field_variable_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster owns semantic kind '%s', but field/variable action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}