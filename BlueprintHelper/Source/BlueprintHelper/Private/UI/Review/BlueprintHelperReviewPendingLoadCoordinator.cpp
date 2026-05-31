// BlueprintHelper ReviewPanel async pending load coordinator implementation.

#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"

#include "Async/Async.h"
#include "HAL/ThreadSafeCounter.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

namespace BlueprintHelperReviewPendingLoad
{
	static bool IsPendingStatus(EBlueprintHelperReviewChangeStatus Status)
	{
		return Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	static TArray<FBlueprintHelperReviewValidityCandidate> BuildValidityCandidates(
		const FBlueprintHelperReviewStoreService* Store,
		const FString& AssetPathFilter,
		const FBlueprintHelperReviewPerformanceSettings& Settings)
	{
		TArray<FBlueprintHelperReviewValidityCandidate> Candidates;
		if (!Settings.bValiditySweepEnabled || !Store)
		{
			return Candidates;
		}

		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.AssetPathFilter = AssetPathFilter;
		Query.bPendingOnly = true;
		Query.bSkipMissingAssetRecords = false;

		const int32 CandidateBudget = FMath::Max(1, Settings.PendingLoadValidityCandidateBudget);
		const int32 SourceSummaryBudget = FMath::Max(1, Settings.ValiditySweepMaxRecordHydrationsPerWorkerBatch);
		int32 ScannedSummaries = 0;
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary :
			Store->QueryPendingVisibleChangeSummaries(Query))
		{
			if (ScannedSummaries >= SourceSummaryBudget || Candidates.Num() >= CandidateBudget)
			{
				break;
			}
			++ScannedSummaries;

			if (!IsPendingStatus(Summary.Change.Status))
			{
				continue;
			}
			for (const FBlueprintHelperReviewAtomicTarget& Target : Summary.Change.AtomicTargets)
			{
				if (Candidates.Num() >= CandidateBudget)
				{
					break;
				}
				if (!IsPendingStatus(Target.Status))
				{
					continue;
				}

				FBlueprintHelperReviewValidityCandidate Candidate;
				Candidate.ReviewRecordId = Summary.ReviewRecordId;
				Candidate.ChangeId = Summary.Change.ChangeId;
				Candidate.AssetPath = Summary.Change.AssetPath.IsEmpty()
					? Summary.RecordAssetPath
					: Summary.Change.AssetPath;
				Candidate.Target = Target;
				Candidates.Add(MoveTemp(Candidate));
			}
		}
		return Candidates;
	}

	static void AddUniqueChange(
		TArray<FBlueprintHelperReviewVisibleChange>& Changes,
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		if (Change.ChangeId.IsEmpty())
		{
			Changes.Add(Change);
			return;
		}
		if (!Changes.ContainsByPredicate([&Change](const FBlueprintHelperReviewVisibleChange& Existing)
		{
			return Existing.ChangeId == Change.ChangeId;
		}))
		{
			Changes.Add(Change);
		}
	}

	static bool EventMatchesSummary(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
	{
		const bool bRecordMatches = Event.ReviewRecordIds.Num() == 0
			|| Event.ReviewRecordIds.Contains(Summary.ReviewRecordId);
		const bool bChangeMatches = Event.ChangeIds.Num() == 0
			|| Event.ChangeIds.Contains(Summary.Change.ChangeId);
		const bool bAssetMatches = Event.AssetPaths.Num() == 0
			|| Event.AssetPaths.Contains(Summary.RecordAssetPath)
			|| Event.AssetPaths.Contains(Summary.Change.AssetPath);
		return bRecordMatches && bChangeMatches && bAssetMatches;
	}

	static TArray<FBlueprintHelperReviewVisibleChange> LoadChangedVisibleChanges(
		const FBlueprintHelperReviewStoreService* Store,
		const FBlueprintHelperReviewPendingLoadRequest& Request)
	{
		TArray<FBlueprintHelperReviewVisibleChange> Changes;
		if (!Store)
		{
			return Changes;
		}
		if (Request.SourceEvent.bRequiresFullReload)
		{
			return Store->LoadPendingVisibleChanges(Request.AssetPathFilter);
		}

		TArray<FString> AssetFilters = Request.SourceEvent.AssetPaths;
		if (!Request.AssetPathFilter.IsEmpty())
		{
			AssetFilters.AddUnique(Request.AssetPathFilter);
		}
		if (AssetFilters.Num() == 0)
		{
			AssetFilters.Add(FString());
		}

		for (const FString& AssetFilter : AssetFilters)
		{
			FBlueprintHelperReviewPendingIndexQuery Query;
			Query.AssetPathFilter = AssetFilter;
			Query.bPendingOnly = true;
			Query.bSkipMissingAssetRecords = false;

			for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary :
				Store->QueryPendingVisibleChangeSummaries(Query))
			{
				if (EventMatchesSummary(Request.SourceEvent, Summary))
				{
					AddUniqueChange(Changes, Summary.Change);
				}
			}
		}
		return Changes;
	}
}

struct FBlueprintHelperReviewPendingLoadCoordinator::FSharedState
{
	FSharedState(
		const FBlueprintHelperReviewStoreService* InReviewStoreService,
		const FBlueprintHelperReviewPerformanceSettings& InReviewPerformanceSettings)
		: ReviewStoreService(InReviewStoreService)
		, ReviewPerformanceSettings(InReviewPerformanceSettings)
	{
	}

	FThreadSafeCounter64 LatestRequestId;
	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	FBlueprintHelperReviewPerformanceSettings ReviewPerformanceSettings;
};

FBlueprintHelperReviewPendingLoadCoordinator::FBlueprintHelperReviewPendingLoadCoordinator(
	const FBlueprintHelperReviewStoreService* InReviewStoreService,
	const FBlueprintHelperReviewPerformanceSettings& InReviewPerformanceSettings)
	: State(MakeShared<FSharedState, ESPMode::ThreadSafe>(
		InReviewStoreService,
		InReviewPerformanceSettings))
{
}

int64 FBlueprintHelperReviewPendingLoadCoordinator::RequestLoad(
	const FBlueprintHelperReviewPendingLoadRequest& Request,
	FBlueprintHelperReviewPendingLoadCompleted OnCompleted)
{
	const int64 RequestId = State->LatestRequestId.Increment();
	TSharedRef<FSharedState, ESPMode::ThreadSafe> SharedState = State;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [SharedState, Request, RequestId, OnCompleted]()
	{
		FBlueprintHelperReviewPendingLoadResult Result;
		Result.RequestId = RequestId;
		Result.Source = Request.Source;
		Result.SourceEvent = Request.SourceEvent;

		if (!SharedState->ReviewStoreService)
		{
			Result.Error = TEXT("review_store_service_unavailable");
		}
		else
		{
			Result.Changes = BlueprintHelperReviewPendingLoad::LoadChangedVisibleChanges(
				SharedState->ReviewStoreService,
				Request);
			Result.ValidityCandidates = BlueprintHelperReviewPendingLoad::BuildValidityCandidates(
				SharedState->ReviewStoreService,
				Request.AssetPathFilter,
				SharedState->ReviewPerformanceSettings);
			Result.bSucceeded = true;
		}

		AsyncTask(ENamedThreads::GameThread, [SharedState, Result, OnCompleted]()
		{
			FBlueprintHelperReviewPendingLoadResult GameThreadResult = Result;
			if (SharedState->LatestRequestId.GetValue() != Result.RequestId)
			{
				GameThreadResult.bDiscarded = true;
			}
			if (OnCompleted.IsBound())
			{
				OnCompleted.Execute(GameThreadResult);
			}
		});
	});

	return RequestId;
}

void FBlueprintHelperReviewPendingLoadCoordinator::CancelPendingLoads()
{
	State->LatestRequestId.Increment();
}
