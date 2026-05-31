// BlueprintHelper ReviewPanel async pending load coordinator.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"
#include "UI/BlueprintHelperUiSettings.h"

class FBlueprintHelperReviewStoreService;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadRequest
{
	FString AssetPathFilter;
	FString Source;
	FBlueprintHelperReviewStoreChangedEvent SourceEvent =
		FBlueprintHelperReviewStoreChangedEvent::FullReload();
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadResult
{
	int64 RequestId = 0;
	bool bSucceeded = false;
	bool bDiscarded = false;
	FString Source;
	FString Error;
	FBlueprintHelperReviewStoreChangedEvent SourceEvent =
		FBlueprintHelperReviewStoreChangedEvent::FullReload();
	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	TArray<FBlueprintHelperReviewValidityCandidate> ValidityCandidates;
};

DECLARE_DELEGATE_OneParam(
	FBlueprintHelperReviewPendingLoadCompleted,
	const FBlueprintHelperReviewPendingLoadResult&);

class BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadCoordinator
{
public:
	explicit FBlueprintHelperReviewPendingLoadCoordinator(
		const FBlueprintHelperReviewStoreService* InReviewStoreService,
		const FBlueprintHelperReviewPerformanceSettings& InReviewPerformanceSettings);

	int64 RequestLoad(
		const FBlueprintHelperReviewPendingLoadRequest& Request,
		FBlueprintHelperReviewPendingLoadCompleted OnCompleted);

	void CancelPendingLoads();

private:
	struct FSharedState;

	TSharedRef<FSharedState, ESPMode::ThreadSafe> State;
};
