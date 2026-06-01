// BlueprintHelper ReviewPanel reject timing model.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"

struct FBlueprintHelperReviewPendingLoadResult;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewRejectTimingSample
{
	bool bValid = false;
	FString Stage;
	FString ChangeId;
	FString AssetPath;
	FString Detail;
	double StageMs = 0.0;
	double TotalMs = 0.0;
	bool bHasChangeSnapshot = false;
	FBlueprintHelperReviewVisibleChange ChangeSnapshot;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewRejectTimingModel
{
public:
	void Begin(const FBlueprintHelperReviewVisibleChange& Change, double NowSeconds);
	bool Contains(const FString& ChangeId) const;
	bool IsWaitingForStoreRefresh(const FString& ChangeId) const;
	void MarkWaitingForStoreRefresh(const FString& ChangeId);
	void CancelWaitingForStoreRefresh(const FString& ChangeId);
	void Complete(const FString& ChangeId);

	FBlueprintHelperReviewRejectTimingSample RecordStage(
		const FString& ChangeId,
		const FString& Stage,
		double NowSeconds,
		const FString& Detail = FString());

	TArray<FBlueprintHelperReviewRejectTimingSample> RecordMatchingStoreEvent(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const FString& Stage,
		double NowSeconds,
		const FString& Detail,
		bool bCompleteMatches);

private:
	struct FState
	{
		FString ChangeId;
		FString AssetPath;
		FBlueprintHelperReviewVisibleChange ChangeSnapshot;
		double StartedAtSeconds = 0.0;
		double LastStageAtSeconds = 0.0;
		bool bWaitingForStoreRefresh = false;
	};

	static bool EventMatchesState(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const FState& State);
	FBlueprintHelperReviewRejectTimingSample MakeSample(
		FState& State,
		const FString& Stage,
		double NowSeconds,
		const FString& Detail);

	TMap<FString, FState> StatesByChangeId;
};
