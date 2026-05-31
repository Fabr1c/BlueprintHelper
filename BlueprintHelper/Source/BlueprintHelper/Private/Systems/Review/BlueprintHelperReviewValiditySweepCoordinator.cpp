// BlueprintHelper Review low-speed validity sweep coordinator implementation.

#include "Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityResolver.h"

namespace BlueprintHelperReviewValiditySweep
{
	static bool IsPendingStatus(EBlueprintHelperReviewChangeStatus Status)
	{
		return Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	static void AddUniqueNonEmpty(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Values.AddUnique(Value);
		}
	}
}

FBlueprintHelperReviewValiditySweepCoordinator::FBlueprintHelperReviewValiditySweepCoordinator(
	const FBlueprintHelperReviewStoreService* InReviewStoreService,
	const FBlueprintHelperReviewPerformanceSettings& InSettings)
	: ReviewStoreService(InReviewStoreService)
	, Settings(InSettings)
{
}

FBlueprintHelperReviewValiditySweepCoordinator::~FBlueprintHelperReviewValiditySweepCoordinator()
{
	Cancel();
}

void FBlueprintHelperReviewValiditySweepCoordinator::StartSweep(const FString& Source)
{
	if (!Settings.bValiditySweepEnabled || !ReviewStoreService)
	{
		return;
	}

	Cancel();
	const int64 SweepId = LatestSweepId.Increment();
	TWeakPtr<FBlueprintHelperReviewValiditySweepCoordinator, ESPMode::ThreadSafe> WeakSelf = AsShared();
	const FBlueprintHelperReviewStoreService* Store = ReviewStoreService;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSelf, Store, SweepId, Source]()
	{
		TArray<FBlueprintHelperReviewValidityCandidate> Candidates;
		if (Store)
		{
			FBlueprintHelperReviewPendingIndexQuery Query;
			Query.bPendingOnly = true;
			Query.bSkipMissingAssetRecords = false;
			const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries =
				Store->QueryPendingVisibleChangeSummaries(Query);
			for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
			{
				if (!BlueprintHelperReviewValiditySweep::IsPendingStatus(Summary.Change.Status))
				{
					continue;
				}
				for (const FBlueprintHelperReviewAtomicTarget& Target : Summary.Change.AtomicTargets)
				{
					if (!BlueprintHelperReviewValiditySweep::IsPendingStatus(Target.Status))
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
		}

		AsyncTask(ENamedThreads::GameThread, [WeakSelf, SweepId, Source, Candidates = MoveTemp(Candidates)]() mutable
		{
			if (TSharedPtr<FBlueprintHelperReviewValiditySweepCoordinator, ESPMode::ThreadSafe> Self = WeakSelf.Pin())
			{
				Self->HandleCandidatesReady(SweepId, Source, MoveTemp(Candidates));
			}
		});
	});
}

void FBlueprintHelperReviewValiditySweepCoordinator::EnqueueCandidatesFromPendingLoad(
	const FString& Source,
	TArray<FBlueprintHelperReviewValidityCandidate> Candidates)
{
	if (!Settings.bValiditySweepEnabled || Candidates.Num() == 0)
	{
		return;
	}

	int64 SweepId = LatestSweepId.GetValue();
	if (SweepId == 0)
	{
		SweepId = LatestSweepId.Increment();
	}
	HandleCandidatesReady(SweepId, Source, MoveTemp(Candidates));
}

void FBlueprintHelperReviewValiditySweepCoordinator::Cancel()
{
	LatestSweepId.Increment();
	PendingCandidates.Reset();
	InvalidResults.Reset();
	SeenCandidateKeys.Reset();
	PendingCandidateIndex = 0;
	ActiveSweepId = 0;
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void FBlueprintHelperReviewValiditySweepCoordinator::HandleCandidatesReady(
	int64 SweepId,
	const FString& Source,
	TArray<FBlueprintHelperReviewValidityCandidate> Candidates)
{
	if (LatestSweepId.GetValue() != SweepId || Candidates.Num() == 0)
	{
		return;
	}

	if (ActiveSweepId != 0 && ActiveSweepId != SweepId)
	{
		return;
	}

	if (ActiveSweepId == 0)
	{
		ActiveSweepId = SweepId;
	}

	if (AppendUniqueCandidates(MoveTemp(Candidates)) == 0)
	{
		return;
	}

	EnsureTickerStarted();
}

int32 FBlueprintHelperReviewValiditySweepCoordinator::AppendUniqueCandidates(
	TArray<FBlueprintHelperReviewValidityCandidate> Candidates)
{
	int32 AddedCount = 0;
	for (FBlueprintHelperReviewValidityCandidate& Candidate : Candidates)
	{
		const FString CandidateKey = MakeCandidateKey(Candidate);
		if (CandidateKey.IsEmpty() || SeenCandidateKeys.Contains(CandidateKey))
		{
			continue;
		}
		SeenCandidateKeys.Add(CandidateKey);
		PendingCandidates.Add(MoveTemp(Candidate));
		++AddedCount;
	}
	return AddedCount;
}

void FBlueprintHelperReviewValiditySweepCoordinator::EnsureTickerStarted()
{
	if (TickerHandle.IsValid())
	{
		return;
	}

	TWeakPtr<FBlueprintHelperReviewValiditySweepCoordinator, ESPMode::ThreadSafe> WeakSelf = AsShared();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakSelf](float DeltaTime)
		{
			if (TSharedPtr<FBlueprintHelperReviewValiditySweepCoordinator, ESPMode::ThreadSafe> Self = WeakSelf.Pin())
			{
				return Self->TickValidation(DeltaTime);
			}
			return false;
		}),
		0.0f);
}

FString FBlueprintHelperReviewValiditySweepCoordinator::MakeCandidateKey(
	const FBlueprintHelperReviewValidityCandidate& Candidate)
{
	if (Candidate.ReviewRecordId.IsEmpty() || Candidate.Target.TargetKey.IsEmpty())
	{
		return FString();
	}
	return Candidate.ReviewRecordId
		+ TEXT("|")
		+ Candidate.ChangeId
		+ TEXT("|")
		+ Candidate.Target.TargetKey;
}

bool FBlueprintHelperReviewValiditySweepCoordinator::TickValidation(float DeltaTime)
{
	if (ActiveSweepId == 0 || LatestSweepId.GetValue() != ActiveSweepId)
	{
		TickerHandle.Reset();
		return false;
	}

	FBlueprintHelperReviewTargetValidityResolver Resolver;
	const double StartSeconds = FPlatformTime::Seconds();
	const int32 MaxTargets = FMath::Max(1, Settings.ValiditySweepMaxGameThreadTargetsPerFrame);
	const double MaxSeconds = FMath::Max(0.05f, Settings.ValiditySweepMaxGameThreadMillisecondsPerFrame) / 1000.0;

	int32 ValidatedThisFrame = 0;
	while (PendingCandidateIndex < PendingCandidates.Num() && ValidatedThisFrame < MaxTargets)
	{
		if ((FPlatformTime::Seconds() - StartSeconds) >= MaxSeconds)
		{
			break;
		}

		const FBlueprintHelperReviewValidityResult Result =
			Resolver.ValidateOnGameThread(PendingCandidates[PendingCandidateIndex]);
		if (!Result.bValid)
		{
			InvalidResults.Add(Result);
		}
		++PendingCandidateIndex;
		++ValidatedThisFrame;

		if (InvalidResults.Num() >= Settings.ValiditySweepMaxInvalidPurgesPerBatch)
		{
			ApplyInvalidResults();
		}
	}

	if (PendingCandidateIndex >= PendingCandidates.Num())
	{
		ApplyInvalidResults();
		TickerHandle.Reset();
		ActiveSweepId = 0;
		PendingCandidates.Reset();
		SeenCandidateKeys.Reset();
		PendingCandidateIndex = 0;
		return false;
	}

	return true;
}

void FBlueprintHelperReviewValiditySweepCoordinator::ApplyInvalidResults()
{
	if (!ReviewStoreService || InvalidResults.Num() == 0)
	{
		InvalidResults.Reset();
		return;
	}

	TMap<FString, TArray<FString>> TargetKeysByRecordId;
	TArray<FString> ChangedRecordIds;
	TArray<FString> ChangedChangeIds;
	TArray<FString> ChangedAssetPaths;

	for (const FBlueprintHelperReviewValidityResult& Result : InvalidResults)
	{
		const FString& ReviewRecordId = Result.Candidate.ReviewRecordId;
		const FString& TargetKey = Result.Candidate.Target.TargetKey;
		if (ReviewRecordId.IsEmpty() || TargetKey.IsEmpty())
		{
			continue;
		}
		TargetKeysByRecordId.FindOrAdd(ReviewRecordId).AddUnique(TargetKey);
		BlueprintHelperReviewValiditySweep::AddUniqueNonEmpty(ChangedRecordIds, ReviewRecordId);
		BlueprintHelperReviewValiditySweep::AddUniqueNonEmpty(ChangedChangeIds, Result.Candidate.ChangeId);
		BlueprintHelperReviewValiditySweep::AddUniqueNonEmpty(ChangedAssetPaths, Result.Candidate.AssetPath);
	}
	InvalidResults.Reset();

	for (const TPair<FString, TArray<FString>>& Pair : TargetKeysByRecordId)
	{
		TArray<FString> DebugCaseIdsToDelete;
		bool bRecordDeleted = false;
		FString Error;
		ReviewStoreService->PurgeReviewTargets(
			Pair.Key,
			Pair.Value,
			DebugCaseIdsToDelete,
			bRecordDeleted,
			Error);
	}

	if (ChangedRecordIds.Num() > 0)
	{
		ReviewStoreService->NotifyPendingReviewChanged(
			FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
				ChangedRecordIds,
				ChangedChangeIds,
				ChangedAssetPaths));
	}
}
