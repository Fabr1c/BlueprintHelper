// BlueprintHelper ReviewPanel async pending load coordinator implementation.

#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"

#include "Async/Async.h"
#include "HAL/ThreadSafeCounter.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndexService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

namespace BlueprintHelperReviewPendingLoad
{
	static bool IsPendingStatus(EBlueprintHelperReviewChangeStatus Status)
	{
		return Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	static TArray<FBlueprintHelperReviewValidityCandidate> BuildValidityCandidatesFromSummaries(
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary>& Summaries,
		const FBlueprintHelperReviewPerformanceSettings& Settings)
	{
		TArray<FBlueprintHelperReviewValidityCandidate> Candidates;
		if (!Settings.bValiditySweepEnabled)
		{
			return Candidates;
		}

		const int32 CandidateBudget = FMath::Max(1, Settings.PendingLoadValidityCandidateBudget);
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
		{
			if (!IsPendingStatus(Summary.Change.Status))
			{
				continue;
			}
			for (const FBlueprintHelperReviewAtomicTarget& Target : Summary.Change.AtomicTargets)
			{
				if (Candidates.Num() >= CandidateBudget)
				{
					return Candidates;
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

	static void AddUniqueSummary(
		TArray<FBlueprintHelperReviewPendingVisibleChangeSummary>& Summaries,
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
	{
		if (Summary.Change.ChangeId.IsEmpty())
		{
			Summaries.Add(Summary);
			return;
		}
		if (!Summaries.ContainsByPredicate([&Summary](const FBlueprintHelperReviewPendingVisibleChangeSummary& Existing)
		{
			return Existing.Change.ChangeId == Summary.Change.ChangeId;
		}))
		{
			Summaries.Add(Summary);
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

	static TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> LoadChangedVisibleChangeSummaries(
		const FBlueprintHelperReviewStoreService* Store,
		const FBlueprintHelperReviewPendingLoadRequest& Request)
	{
		TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries;
		if (!Store)
		{
			return Summaries;
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
					AddUniqueSummary(Summaries, Summary);
				}
			}
		}
		return Summaries;
	}

	static bool LoadPendingVisibleChangePage(
		const FBlueprintHelperReviewPendingLoadRequest& Request,
		FBlueprintHelperReviewPendingIndexPage& OutPage,
		FString& OutError)
	{
		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.AssetPathFilter = Request.AssetPathFilter;
		Query.bPendingOnly = true;
		Query.bSkipMissingAssetRecords = Request.AssetPathFilter.IsEmpty();

		FBlueprintHelperReviewPendingIndexPageRequest PageRequest;
		PageRequest.Query = Query;
		PageRequest.Cursor = Request.Cursor;
		PageRequest.PageSize = Request.PageSize;

		FBlueprintHelperReviewPendingIndexService IndexService;
		return IndexService.QueryPendingVisibleChangePage(PageRequest, OutPage, OutError);
	}

	static TArray<FBlueprintHelperReviewVisibleChange> MakeVisibleChangesFromSummaries(
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary>& Summaries)
	{
		TArray<FBlueprintHelperReviewVisibleChange> Changes;
		Changes.Reserve(Summaries.Num());
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
		{
			Changes.Add(Summary.Change);
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
		Result.Mode = Request.Mode;

		if (!SharedState->ReviewStoreService)
		{
			Result.Error = TEXT("review_store_service_unavailable");
		}
		else
		{
			if (Request.Mode == EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
				|| Request.Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage)
			{
				FBlueprintHelperReviewPendingIndexPage Page;
				FString PageError;
				if (BlueprintHelperReviewPendingLoad::LoadPendingVisibleChangePage(Request, Page, PageError))
				{
					Result.Changes = BlueprintHelperReviewPendingLoad::MakeVisibleChangesFromSummaries(Page.Changes);
					Result.ValidityCandidates =
						BlueprintHelperReviewPendingLoad::BuildValidityCandidatesFromSummaries(
							Page.Changes,
							SharedState->ReviewPerformanceSettings);
					Result.NextCursor = Page.NextCursor;
					Result.TotalMatchingCount = Page.TotalMatchingCount;
					Result.bHasMore = Page.bHasMore;
					Result.bSucceeded = true;
				}
				else
				{
					Result.Error = PageError;
				}
			}
			else
			{
				const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> ChangedSummaries =
					BlueprintHelperReviewPendingLoad::LoadChangedVisibleChangeSummaries(
						SharedState->ReviewStoreService,
						Request);
				Result.Changes =
					BlueprintHelperReviewPendingLoad::MakeVisibleChangesFromSummaries(ChangedSummaries);
				Result.ValidityCandidates =
					BlueprintHelperReviewPendingLoad::BuildValidityCandidatesFromSummaries(
						ChangedSummaries,
						SharedState->ReviewPerformanceSettings);
				Result.TotalMatchingCount = Result.Changes.Num();
				Result.bSucceeded = true;
			}
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
