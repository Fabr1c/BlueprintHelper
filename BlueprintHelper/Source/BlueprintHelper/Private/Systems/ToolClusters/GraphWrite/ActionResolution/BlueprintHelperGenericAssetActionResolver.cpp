#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
static FBlueprintHelperActionResolutionResult MakeInvalidResult(const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeNotFoundResult(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("asset_action_spawner_not_found");
	Result.Message = Message;
	Result.MatchReason = FString::Printf(
		TEXT("asset_action_spawner_not_found stable_id=%s owner=%s node=%s signature=%s query=%s menu=%s category=%s"),
		Evidence.StableId.IsEmpty() ? TEXT("none") : *Evidence.StableId,
		Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath,
		Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
		Evidence.SpawnerSignature.IsEmpty() ? TEXT("none") : *Evidence.SpawnerSignature,
		Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
		Evidence.MenuName.IsEmpty() ? TEXT("none") : *Evidence.MenuName,
		Evidence.Category.IsEmpty() ? TEXT("none") : *Evidence.Category);
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeAmbiguousResult(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	int32 MatchCount)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("asset_action_spawner_ambiguous");
	Result.Message = FString::Printf(
		TEXT("asset_action projected evidence matched %d ActionDatabase spawners. Provide asset_action_stable_id or stronger projected evidence. query=%s node=%s owner=%s"),
		MatchCount,
		Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
		Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
		Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath);
	Result.MatchReason = FString::Printf(
		TEXT("asset_action_spawner_ambiguous count=%d query=%s node=%s owner=%s"),
		MatchCount,
		Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
		Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
		Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath);
	return Result;
}

static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
	const FBlueprintHelperAssetActionProjectedCandidate& Match)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Match.StableId;
	Candidate.DisplayName = Match.MenuName.IsEmpty() ? TEXT("asset_action") : Match.MenuName;
	Candidate.Category = Match.Category;
	Candidate.NodeClassPath = Match.NodeClassPath;
	Candidate.MatchReason = FString::Printf(
		TEXT("action_database owner=%s node=%s menu=%s"),
		Match.OwnerPath.IsEmpty() ? TEXT("none") : *Match.OwnerPath,
		Match.NodeClassPath.IsEmpty() ? TEXT("none") : *Match.NodeClassPath,
		Match.MenuName.IsEmpty() ? TEXT("none") : *Match.MenuName);
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Match.Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	return Candidate;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView&)
{
	const FBlueprintHelperProjectedAssetActionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(Request);
	if (!Evidence.HasProjectedIdentity())
	{
		return MakeInvalidResult(TEXT("asset_action create requires projected ActionDatabase spawner identity evidence."));
	}

	FBlueprintHelperAssetActionProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Request.Blueprint;
	ProjectionRequest.TargetGraph = Request.TargetGraph;
	ProjectionRequest.RequiredEvidence = Evidence;

	const FBlueprintHelperAssetActionProjectionResult Projection =
		FBlueprintHelperAssetActionProjectionService::Project(ProjectionRequest);

	if (Projection.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest)
	{
		return MakeInvalidResult(Projection.Message.IsEmpty()
			? TEXT("asset_action create requires projected ActionDatabase spawner evidence.")
			: Projection.Message);
	}
	if (Projection.Candidates.Num() == 0)
	{
		return MakeNotFoundResult(
			Evidence,
			TEXT("asset_action projected evidence did not match any ActionDatabase spawner."));
	}
	if (Projection.Candidates.Num() > 1)
	{
		return MakeAmbiguousResult(Evidence, Projection.Candidates.Num());
	}

	const FBlueprintHelperAssetActionProjectedCandidate& Match = Projection.Candidates[0];
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Match.StableId;
	Result.SelectedSpawner = Match.Spawner;
	Result.CandidateActions.Add(MakeCandidateInfo(Match));
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
