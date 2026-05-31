// BlueprintHelper Review low-speed validity sweep coordinator.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/ThreadSafeCounter.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"
#include "UI/BlueprintHelperUiSettings.h"

class FBlueprintHelperReviewStoreService;

class BLUEPRINTHELPER_API FBlueprintHelperReviewValiditySweepCoordinator
	: public TSharedFromThis<FBlueprintHelperReviewValiditySweepCoordinator, ESPMode::ThreadSafe>
{
public:
	FBlueprintHelperReviewValiditySweepCoordinator(
		const FBlueprintHelperReviewStoreService* InReviewStoreService,
		const FBlueprintHelperReviewPerformanceSettings& InSettings);

	~FBlueprintHelperReviewValiditySweepCoordinator();

	void StartSweep(const FString& Source);
	void EnqueueCandidatesFromPendingLoad(
		const FString& Source,
		TArray<FBlueprintHelperReviewValidityCandidate> Candidates);
	void Cancel();

private:
	void HandleCandidatesReady(
		int64 SweepId,
		const FString& Source,
		TArray<FBlueprintHelperReviewValidityCandidate> Candidates);
	int32 AppendUniqueCandidates(TArray<FBlueprintHelperReviewValidityCandidate> Candidates);
	void EnsureTickerStarted();
	bool TickValidation(float DeltaTime);
	void ApplyInvalidResults();
	static FString MakeCandidateKey(const FBlueprintHelperReviewValidityCandidate& Candidate);

	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	FBlueprintHelperReviewPerformanceSettings Settings;
	FThreadSafeCounter64 LatestSweepId;
	TArray<FBlueprintHelperReviewValidityCandidate> PendingCandidates;
	TArray<FBlueprintHelperReviewValidityResult> InvalidResults;
	TSet<FString> SeenCandidateKeys;
	FTSTicker::FDelegateHandle TickerHandle;
	int32 PendingCandidateIndex = 0;
	int64 ActiveSweepId = 0;
};
