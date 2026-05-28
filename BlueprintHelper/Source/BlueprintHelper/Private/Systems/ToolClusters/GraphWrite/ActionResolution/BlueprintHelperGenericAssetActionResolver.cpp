#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView&)
{
	const FBlueprintHelperProjectedAssetActionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(Request);
	if (!Evidence.HasProjectedIdentity())
	{
		return UGraphWriteActionClusterUtils::MakeClusterInvalidResult(TEXT("asset_action create requires projected ActionDatabase spawner identity evidence."));
	}

	FBlueprintHelperAssetActionProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Request.Blueprint;
	ProjectionRequest.TargetGraph = Request.TargetGraph;
	ProjectionRequest.RequiredEvidence = Evidence;

	const FBlueprintHelperAssetActionProjectionResult Projection =
		FBlueprintHelperAssetActionProjectionService::Project(ProjectionRequest);

	if (Projection.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest)
	{
		return UGraphWriteActionClusterUtils::MakeClusterInvalidResult(Projection.Message.IsEmpty()
			? TEXT("asset_action create requires projected ActionDatabase spawner evidence.")
			: Projection.Message);
	}
	if (Projection.Candidates.Num() == 0)
	{
		return UGraphWriteActionClusterUtils::MakeNotFoundResult(
			Evidence,
			TEXT("asset_action projected evidence did not match any ActionDatabase spawner."));
	}
	if (Projection.Candidates.Num() > 1)
	{
		return UGraphWriteActionClusterUtils::MakeAmbiguousResult(Evidence, Projection.Candidates.Num());
	}

	const FBlueprintHelperAssetActionProjectedCandidate& Match = Projection.Candidates[0];
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Match.StableId;
	Result.SelectedSpawner = Match.Spawner;
	Result.CandidateActions.Add(UGraphWriteActionClusterUtils::MakeCandidateInfo(Match));
	Result.SpawnerClass = Match.Spawner ? Match.Spawner->GetClass()->GetPathName() : FString();
	Result.NodeClass = Match.NodeClassPath;
	Result.MatchReason = FString::Printf(
		TEXT("action_database stable_id=%s"),
		*Match.StableId);
	Result.Message = FString::Printf(
		TEXT("Resolved asset_action from ActionDatabase spawner '%s'."),
		*Match.StableId);
	return Result;
}
