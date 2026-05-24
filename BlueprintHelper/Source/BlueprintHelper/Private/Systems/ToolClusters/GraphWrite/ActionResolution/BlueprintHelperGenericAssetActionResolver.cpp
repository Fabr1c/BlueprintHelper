#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
struct FAssetActionDatabaseMatch
{
	const UObject* ActionOwner = nullptr;
	UBlueprintNodeSpawner* Spawner = nullptr;
	UClass* NodeClass = nullptr;
	FString StableId;
	FString MenuName;
	FString Category;
};

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

static bool MatchesQueryEvidence(const FString& Query, const FAssetActionDatabaseMatch& Match)
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
		*Match.StableId,
		Match.ActionOwner ? *Match.ActionOwner->GetPathName() : TEXT(""),
		Match.NodeClass ? *Match.NodeClass->GetPathName() : TEXT(""),
		*Match.MenuName,
		*Match.Category));
	return SearchText.Contains(NormalizedQuery);
}

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

static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(const FAssetActionDatabaseMatch& Match)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Match.StableId;
	Candidate.DisplayName = Match.MenuName.IsEmpty() ? TEXT("asset_action") : Match.MenuName;
	Candidate.Category = Match.Category;
	Candidate.NodeClassPath = Match.NodeClass ? Match.NodeClass->GetPathName() : FString();
	Candidate.MatchReason = FString::Printf(
		TEXT("action_database owner=%s node=%s menu=%s"),
		Match.ActionOwner ? *Match.ActionOwner->GetPathName() : TEXT("none"),
		Match.NodeClass ? *Match.NodeClass->GetPathName() : TEXT("none"),
		Match.MenuName.IsEmpty() ? TEXT("none") : *Match.MenuName);
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Match.Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	return Candidate;
}

static bool TryBuildMatch(
	const FBlueprintActionContext& ActionContext,
	const UObject* ActionOwner,
	UBlueprintNodeSpawner* Spawner,
	FAssetActionDatabaseMatch& OutMatch)
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

	OutMatch.ActionOwner = ActionOwner;
	OutMatch.Spawner = Spawner;
	OutMatch.NodeClass = NodeClass;
	OutMatch.StableId = FBlueprintHelperProjectedSpawnerEvidence::MakeAssetActionStableId(ActionOwner, Spawner, NodeClass);
	OutMatch.MenuName = UiSpec.MenuName.ToString().TrimStartAndEnd();
	OutMatch.Category = UiSpec.Category.ToString().TrimStartAndEnd();
	return true;
}

static bool MatchesProjectedEvidence(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	const FAssetActionDatabaseMatch& Match)
{
	if (!MatchesExactEvidence(Evidence.StableId, Match.StableId))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.NodeClassPath, Match.NodeClass ? Match.NodeClass->GetPathName() : FString()))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.SpawnerSignature, Match.Spawner ? Match.Spawner->GetSpawnerSignature().ToString() : FString()))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.OwnerPath, Match.ActionOwner ? Match.ActionOwner->GetPathName() : FString()))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.MenuName, Match.MenuName))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.Category, Match.Category))
	{
		return false;
	}
	return MatchesQueryEvidence(Evidence.Query, Match);
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView&)
{
	const FBlueprintHelperProjectedAssetActionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(Request);
	if (!Evidence.HasSelector())
	{
		return MakeInvalidResult(TEXT("asset_action create requires projected ActionDatabase spawner evidence."));
	}

	FBlueprintActionContext ActionContext;
	if (Request.Blueprint)
	{
		ActionContext.Blueprints.Add(Request.Blueprint);
	}
	if (Request.TargetGraph)
	{
		ActionContext.Graphs.Add(Request.TargetGraph);
	}

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = ActionContext;

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& Registry =
		FBlueprintActionDatabase::Get().GetAllActions();

	TArray<FAssetActionDatabaseMatch> Matches;
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

			FAssetActionDatabaseMatch Match;
			if (!TryBuildMatch(ActionContext, ActionOwner, Spawner, Match))
			{
				continue;
			}
			if (!MatchesProjectedEvidence(Evidence, Match))
			{
				continue;
			}

			Matches.Add(MoveTemp(Match));
		}
	}

	if (Matches.Num() == 0)
	{
		return MakeNotFoundResult(
			Evidence,
			TEXT("asset_action projected evidence did not match any ActionDatabase spawner."));
	}
	if (Matches.Num() > 1)
	{
		return MakeAmbiguousResult(Evidence, Matches.Num());
	}

	const FAssetActionDatabaseMatch& Match = Matches[0];
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Match.StableId;
	Result.SelectedSpawner = Match.Spawner;
	Result.CandidateActions.Add(MakeCandidateInfo(Match));
	Result.SpawnerClass = Match.Spawner ? Match.Spawner->GetClass()->GetPathName() : FString();
	Result.NodeClass = Match.NodeClass ? Match.NodeClass->GetPathName() : FString();
	Result.MatchReason = FString::Printf(
		TEXT("action_database stable_id=%s"),
		*Match.StableId);
	Result.Message = FString::Printf(
		TEXT("Resolved asset_action from ActionDatabase spawner '%s'."),
		*Match.StableId);
	return Result;
}
