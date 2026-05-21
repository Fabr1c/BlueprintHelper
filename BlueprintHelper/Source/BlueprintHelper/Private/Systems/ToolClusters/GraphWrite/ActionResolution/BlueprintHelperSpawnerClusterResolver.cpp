#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperSpawnerClusterResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	switch (Request.ClusterKind)
	{
	case EBlueprintHelperSpawnerClusterKind::FunctionAction:
		return FBlueprintHelperFunctionActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::FieldVariableAction:
		return FBlueprintHelperFieldVariableActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::EventDelegateAction:
		return FBlueprintHelperEventDelegateActionCluster::Resolve(Request);
	case EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction:
		return FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(Request);
	default:
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.ErrorCode = TEXT("unsupported_spawner_cluster");
		Result.Message = FString::Printf(
			TEXT("ActionResolution does not have a resolver for cluster '%s' with semantic '%s'."),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(Request.ClusterKind),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
		return Result;
	}
}