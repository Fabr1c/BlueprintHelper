#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionAdapterUtils.h"

FBlueprintHelperAssetActionProjectionResult FBlueprintHelperAssetActionProjectionService::Project(
	const FBlueprintHelperAssetActionProjectionRequest& Request)
{
	FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Request.Blueprint;
	ProjectionRequest.TargetGraph = Request.TargetGraph;
	ProjectionRequest.RequiredEvidence = UGraphWriteActionAdapterUtils::ToNeutralEvidence(Request.RequiredEvidence);
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
		Result.Candidates.Add(UGraphWriteActionAdapterUtils::ToAssetCandidate(Candidate));
	}
	return Result;
}
