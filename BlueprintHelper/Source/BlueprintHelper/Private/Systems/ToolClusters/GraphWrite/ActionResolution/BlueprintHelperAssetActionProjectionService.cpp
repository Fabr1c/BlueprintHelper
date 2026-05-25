#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"

namespace
{
static FBlueprintHelperActionDatabaseProjectionEvidence ToNeutralEvidence(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	FBlueprintHelperActionDatabaseProjectionEvidence Result;
	Result.StableId = Evidence.StableId;
	Result.NodeClassPath = Evidence.NodeClassPath;
	Result.SpawnerSignature = Evidence.SpawnerSignature;
	Result.OwnerPath = Evidence.OwnerPath;
	Result.Query = Evidence.Query;
	Result.MenuName = Evidence.MenuName;
	Result.Category = Evidence.Category;
	return Result;
}

static FBlueprintHelperAssetActionProjectedCandidate ToAssetCandidate(
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	FBlueprintHelperAssetActionProjectedCandidate Result;
	Result.ActionOwner = Candidate.ActionOwner;
	Result.Spawner = Candidate.Spawner;
	Result.NodeClass = Candidate.NodeClass;
	Result.StableId = Candidate.StableId;
	Result.NodeClassPath = Candidate.NodeClassPath;
	Result.SpawnerSignature = Candidate.SpawnerSignature;
	Result.OwnerPath = Candidate.OwnerPath;
	Result.Query = Candidate.Query;
	Result.MenuName = Candidate.MenuName;
	Result.Category = Candidate.Category;
	return Result;
}
}

FBlueprintHelperAssetActionProjectionResult FBlueprintHelperAssetActionProjectionService::Project(
	const FBlueprintHelperAssetActionProjectionRequest& Request)
{
	FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Request.Blueprint;
	ProjectionRequest.TargetGraph = Request.TargetGraph;
	ProjectionRequest.RequiredEvidence = ToNeutralEvidence(Request.RequiredEvidence);
	ProjectionRequest.Query = Request.Query;
	ProjectionRequest.ErrorPrefix = TEXT("asset_action");

	const FBlueprintHelperActionDatabaseProjectionResult Projection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);

	FBlueprintHelperAssetActionProjectionResult Result;
	Result.Status = Projection.Status;
	Result.ErrorCode = Projection.ErrorCode;
	Result.Message = Projection.Message;
	for (const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate : Projection.Candidates)
	{
		Result.Candidates.Add(ToAssetCandidate(Candidate));
	}
	return Result;
}
