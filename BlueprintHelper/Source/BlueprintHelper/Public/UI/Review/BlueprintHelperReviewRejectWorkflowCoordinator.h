// BlueprintHelper Review reject workflow coordinator.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"
#include "UI/Review/BlueprintHelperReviewRejectTimingModel.h"
#include "UI/Review/BlueprintHelperReviewRejectWorkflowModel.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewRejectWorkflowCoordinator
	: public TSharedFromThis<FBlueprintHelperReviewRejectWorkflowCoordinator>
{
public:
	struct FCallbacks
	{
		TFunction<TOptional<FBlueprintHelperReviewVisibleChange>(const FString&)> ResolveChangeSnapshot;
		TFunction<void(const FString&)> OnChangeMissing;
		TFunction<void(const FString&, const FBlueprintHelperReviewVisibleChange&)> OnPrepareStarted;
		TFunction<void(const FString&, const FBlueprintHelperReviewRejectOptions&)> OnPrepareFinished;
		TFunction<void(const FString&)> ExecutePreparedMutation;
	};

	void SetCallbacks(FCallbacks InCallbacks);
	void EnqueueReject(const FString& ChangeId);
	void StartNextPrepare();
	void HandlePreparedRejectReady(const FString& ChangeId, const FBlueprintHelperReviewRejectOptions& PreparedOptions);
	const FBlueprintHelperReviewRejectOptions* FindPreparedOptions(const FString& ChangeId) const;
	void FinishReject(const FString& ChangeId);

	void BeginTiming(const FBlueprintHelperReviewVisibleChange& Change, double NowSeconds);
	bool ContainsTiming(const FString& ChangeId) const;
	bool IsWaitingForStoreRefresh(const FString& ChangeId) const;
	void MarkWaitingForStoreRefresh(const FString& ChangeId);
	void CancelWaitingForStoreRefresh(const FString& ChangeId);
	void CompleteTiming(const FString& ChangeId);
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
	FCallbacks Callbacks;
	FBlueprintHelperReviewRejectWorkflowModel WorkflowModel;
	FBlueprintHelperReviewRejectTimingModel TimingModel;
};
