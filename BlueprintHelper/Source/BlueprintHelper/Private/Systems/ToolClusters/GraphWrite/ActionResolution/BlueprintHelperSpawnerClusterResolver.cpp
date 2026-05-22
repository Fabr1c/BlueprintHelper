#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperSpawnerClusterResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	switch (Request.ClusterKind)
	{
	case EBlueprintHelperSpawnerClusterKind::FunctionAction:
		return FBlueprintHelperFunctionActionCluster::Resolve(Request, Context);
	case EBlueprintHelperSpawnerClusterKind::FieldVariableAction:
		return FBlueprintHelperFieldVariableActionCluster::Resolve(Request, Context);
	case EBlueprintHelperSpawnerClusterKind::EventDelegateAction:
		return FBlueprintHelperEventDelegateActionCluster::Resolve(Request, Context);
	case EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction:
		return FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(Request, Context);
	default:
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.ErrorCode = TEXT("unknown_spawner_cluster");
		Result.Message = FString::Printf(
			TEXT("ActionResolution does not have a resolver for cluster '%s' with semantic '%s'."),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(Request.ClusterKind),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
		return Result;
	}
}
