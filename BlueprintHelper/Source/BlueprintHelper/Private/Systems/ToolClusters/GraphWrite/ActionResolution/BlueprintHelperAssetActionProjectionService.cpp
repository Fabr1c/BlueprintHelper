#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"

namespace
{
static FString NormalizeAssetActionText(const FString& Value)
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
	const FBlueprintHelperAssetActionProjectedCandidate& Candidate)
{
	if (Query.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	const FString NormalizedQuery = NormalizeAssetActionText(Query);
	if (NormalizedQuery.IsEmpty())
	{
		return true;
	}

	const FString SearchText = NormalizeAssetActionText(FString::Printf(
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
	FBlueprintHelperAssetActionProjectedCandidate& OutCandidate)
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
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	const FBlueprintHelperAssetActionProjectedCandidate& Candidate)
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

static FBlueprintHelperProjectedAssetActionEvidence BuildEffectiveEvidence(
	const FBlueprintHelperAssetActionProjectionRequest& Request)
{
	FBlueprintHelperProjectedAssetActionEvidence Evidence = Request.RequiredEvidence;
	if (Evidence.Query.IsEmpty())
	{
		Evidence.Query = Request.Query.TrimStartAndEnd();
	}
	return Evidence;
}

static FBlueprintHelperAssetActionProjectionResult MakeProjectionFailure(
	EBlueprintHelperActionResolutionStatus Status,
	const TCHAR* ErrorCode,
	const FString& Message)
{
	FBlueprintHelperAssetActionProjectionResult Result;
	Result.Status = Status;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}
}

FBlueprintHelperAssetActionProjectionResult FBlueprintHelperAssetActionProjectionService::Project(
	const FBlueprintHelperAssetActionProjectionRequest& Request)
{
	if (!Request.Blueprint || !Request.TargetGraph)
	{
		return MakeProjectionFailure(
			EBlueprintHelperActionResolutionStatus::InvalidRequest,
			TEXT("invalid_asset_action_projection_context"),
			TEXT("asset_action projection requires blueprint and target graph."));
	}

	const FBlueprintHelperProjectedAssetActionEvidence Evidence = BuildEffectiveEvidence(Request);
	if (!Evidence.HasSelector())
	{
		return MakeProjectionFailure(
			EBlueprintHelperActionResolutionStatus::InvalidRequest,
			TEXT("needs_more_semantic_context"),
			TEXT("asset_action projection requires query or projected ActionDatabase spawner evidence."));
	}

	FBlueprintActionContext ActionContext;
	ActionContext.Blueprints.Add(Request.Blueprint);
	ActionContext.Graphs.Add(Request.TargetGraph);

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = ActionContext;

	FBlueprintHelperAssetActionProjectionResult Result;

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

			FBlueprintHelperAssetActionProjectedCandidate Candidate;
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
		Result.ErrorCode = TEXT("asset_action_spawner_not_found");
		Result.Message = TEXT("asset_action projected evidence did not match any current ActionDatabase spawner.");
		return Result;
	}
	if (Result.Candidates.Num() > 1)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
		Result.ErrorCode = TEXT("asset_action_spawner_ambiguous");
		Result.Message = FString::Printf(
			TEXT("asset_action projected evidence matched %d current ActionDatabase spawners."),
			Result.Candidates.Num());
		return Result;
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.Message = TEXT("asset_action projected evidence matched one current ActionDatabase spawner.");
	return Result;
}
