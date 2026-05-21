#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	return OwnsSemanticKind(Request.Semantic.Kind)
		? MakeUnsupportedClusterMigrationResult(Request)
		: MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperGenericAssetStructControlActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
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
	Result.ErrorCode = TEXT("unsupported_generic_asset_struct_control_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("GenericAssetStructControlActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("generic_asset_struct_control_action_cluster_migration_pending");
	Result.Message = FString::Printf(
		TEXT("GenericAssetStructControlActionCluster owns semantic kind '%s', but generic asset/struct/control action resolution has not been migrated yet; no fallback direct node creation was attempted."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}