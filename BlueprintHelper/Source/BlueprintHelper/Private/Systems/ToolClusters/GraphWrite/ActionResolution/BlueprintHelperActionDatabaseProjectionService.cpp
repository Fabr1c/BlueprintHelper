#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionAdapterUtils.h"

bool FBlueprintHelperActionDatabaseProjectionEvidence::HasSelector() const
{
	return !StableId.IsEmpty()
		|| !NodeClassPath.IsEmpty()
		|| !SpawnerSignature.IsEmpty()
		|| !OwnerPath.IsEmpty()
		|| !Query.IsEmpty()
		|| !MenuName.IsEmpty()
		|| !Category.IsEmpty();
}

FBlueprintHelperActionDatabaseProjectionResult FBlueprintHelperActionDatabaseProjectionService::Project(
	const FBlueprintHelperActionDatabaseProjectionRequest& Request)
{
	const FString ErrorPrefix = Request.ErrorPrefix.IsEmpty() ? TEXT("action_database") : Request.ErrorPrefix;
	if (!Request.Blueprint || !Request.TargetGraph)
	{
		return UGraphWriteActionAdapterUtils::MakeProjectionFailure(
			EBlueprintHelperActionResolutionStatus::InvalidRequest,
			FString::Printf(TEXT("invalid_%s_projection_context"), *ErrorPrefix),
			FString::Printf(TEXT("%s projection requires blueprint and target graph."), *ErrorPrefix));
	}

	const FBlueprintHelperActionDatabaseProjectionEvidence Evidence = UGraphWriteActionAdapterUtils::BuildEffectiveEvidence(Request);
	if (!Evidence.HasSelector())
	{
		return UGraphWriteActionAdapterUtils::MakeProjectionFailure(
			EBlueprintHelperActionResolutionStatus::InvalidRequest,
			TEXT("needs_more_semantic_context"),
			FString::Printf(TEXT("%s projection requires query or projected ActionDatabase spawner evidence."), *ErrorPrefix));
	}

	FBlueprintActionContext ActionContext;
	ActionContext.Blueprints.Add(Request.Blueprint);
	ActionContext.Graphs.Add(Request.TargetGraph);

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = ActionContext;

	FBlueprintHelperActionDatabaseProjectionResult Result;

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& Registry =
		FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		const UObject* ActionOwner = Pair.Key.ResolveObjectPtr();
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Pair.Value)
		{
			UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
			if (Filter.IsFiltered(ActionInfo))
			{
				continue;
			}

			FBlueprintHelperActionDatabaseProjectedCandidate Candidate;
			if (!UGraphWriteActionAdapterUtils::TryBuildCandidate(ActionContext, ActionOwner, Spawner, Candidate))
			{
				continue;
			}
			Candidate.Query = Evidence.Query;
			if (!UGraphWriteActionAdapterUtils::MatchesProjectedEvidence(Evidence, Candidate))
			{
				continue;
			}

			Result.Candidates.Add(MoveTemp(Candidate));
		}
	}

	if (Result.Candidates.Num() == 0)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = FString::Printf(TEXT("%s_spawner_not_found"), *ErrorPrefix);
		Result.Message = FString::Printf(TEXT("%s projected evidence did not match any current ActionDatabase spawner."), *ErrorPrefix);
		return Result;
	}
	if (Result.Candidates.Num() > 1)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
		Result.ErrorCode = FString::Printf(TEXT("%s_spawner_ambiguous"), *ErrorPrefix);
		Result.Message = FString::Printf(
			TEXT("%s projected evidence matched %d current ActionDatabase spawners."),
			*ErrorPrefix,
			Result.Candidates.Num());
		return Result;
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.Message = FString::Printf(TEXT("%s projected evidence matched one current ActionDatabase spawner."), *ErrorPrefix);
	return Result;
}
