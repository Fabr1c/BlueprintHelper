#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
static FString NormalizeProjectionText(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd().ToLower();
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT("|"), TEXT(""));
	Result.ReplaceInline(TEXT("/"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	return Result;
}

static bool MatchesExactEvidence(const FString& Expected, const FString& Actual)
{
	return Expected.IsEmpty() || Expected.Equals(Actual.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

static bool MatchesQueryEvidence(
	const FString& Query,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	if (Query.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	const FString NormalizedQuery = NormalizeProjectionText(Query);
	if (NormalizedQuery.IsEmpty())
	{
		return true;
	}

	const FString SearchText = NormalizeProjectionText(FString::Printf(
		TEXT("%s %s %s %s %s"),
		*Candidate.StableId,
		*Candidate.OwnerPath,
		*Candidate.NodeClassPath,
		*Candidate.MenuName,
		*Candidate.Category));
	return SearchText.Contains(NormalizedQuery);
}

static bool TryBuildCandidate(
	const FBlueprintActionContext& ActionContext,
	const UObject* ActionOwner,
	UBlueprintNodeSpawner* Spawner,
	FBlueprintHelperActionDatabaseProjectedCandidate& OutCandidate)
{
	if (!Spawner)
	{
		return false;
	}

	FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
	UClass* NodeClass = const_cast<UClass*>(ActionInfo.GetNodeClass());
	if (!NodeClass)
	{
		return false;
	}

	const FBlueprintActionUiSpec UiSpec =
		Spawner->GetUiSpec(ActionContext, ActionInfo.GetBindings());

	OutCandidate.ActionOwner = ActionOwner;
	OutCandidate.Spawner = Spawner;
	OutCandidate.NodeClass = NodeClass;
	OutCandidate.StableId = FBlueprintHelperProjectedSpawnerEvidence::MakeAssetActionStableId(ActionOwner, Spawner, NodeClass);
	OutCandidate.NodeClassPath = NodeClass->GetPathName();
	OutCandidate.SpawnerSignature = Spawner->GetSpawnerSignature().ToString();
	OutCandidate.OwnerPath = ActionOwner ? ActionOwner->GetPathName() : FString();
	OutCandidate.MenuName = UiSpec.MenuName.ToString().TrimStartAndEnd();
	OutCandidate.Category = UiSpec.Category.ToString().TrimStartAndEnd();
	return true;
}

static bool MatchesProjectedEvidence(
	const FBlueprintHelperActionDatabaseProjectionEvidence& Evidence,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	if (!MatchesExactEvidence(Evidence.StableId, Candidate.StableId))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.NodeClassPath, Candidate.NodeClassPath))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.SpawnerSignature, Candidate.SpawnerSignature))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.OwnerPath, Candidate.OwnerPath))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.MenuName, Candidate.MenuName))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.Category, Candidate.Category))
	{
		return false;
	}
	return MatchesQueryEvidence(Evidence.Query, Candidate);
}

static FBlueprintHelperActionDatabaseProjectionEvidence BuildEffectiveEvidence(
	const FBlueprintHelperActionDatabaseProjectionRequest& Request)
{
	FBlueprintHelperActionDatabaseProjectionEvidence Evidence = Request.RequiredEvidence;
	if (Evidence.Query.IsEmpty())
	{
		Evidence.Query = Request.Query.TrimStartAndEnd();
	}
	return Evidence;
}

static FBlueprintHelperActionDatabaseProjectionResult MakeProjectionFailure(
	EBlueprintHelperActionResolutionStatus Status,
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionDatabaseProjectionResult Result;
	Result.Status = Status;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}
}

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
		return MakeProjectionFailure(
			EBlueprintHelperActionResolutionStatus::InvalidRequest,
			FString::Printf(TEXT("invalid_%s_projection_context"), *ErrorPrefix),
			FString::Printf(TEXT("%s projection requires blueprint and target graph."), *ErrorPrefix));
	}

	const FBlueprintHelperActionDatabaseProjectionEvidence Evidence = BuildEffectiveEvidence(Request);
	if (!Evidence.HasSelector())
	{
		return MakeProjectionFailure(
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
			if (!TryBuildCandidate(ActionContext, ActionOwner, Spawner, Candidate))
			{
				continue;
			}
			Candidate.Query = Evidence.Query;
			if (!MatchesProjectedEvidence(Evidence, Candidate))
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
