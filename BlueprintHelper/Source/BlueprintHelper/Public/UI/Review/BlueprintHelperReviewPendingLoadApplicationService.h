// BlueprintHelper ReviewPanel pending load application service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadRequestApplication
{
	bool bShouldRequestLoad = false;
	bool bShouldCancelPendingLoads = false;
	bool bShouldFinishInFlightRequest = false;
	bool bShouldRecordStoreTiming = false;
	FBlueprintHelperReviewPendingLoadRequest Request;
	FString DebugMessage;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadResultApplication
{
	bool bShouldIgnore = false;
	bool bShouldRecordTiming = false;
	bool bTimingSucceeded = false;
	FString TimingStage;
	FString TimingSource;
	bool bShouldDispatchValidityCandidates = false;
	bool bReturnedMoreThanPageSize = false;
	bool bSignatureUnchanged = false;
	bool bShouldApplyVisibleChanges = false;
	bool bShouldRefreshMainWorkspace = false;
	bool bShouldInvalidate = false;
	FString RecommendedSelectedChangeId;
	FString NewRefreshSignature;
	FString DebugMessage;
	TArray<FBlueprintHelperReviewVisibleChange> NextChanges;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadApplicationService
{
public:
	static FBlueprintHelperReviewStoreChangedEvent NormalizeStoreChangedEvent(
		const FBlueprintHelperReviewStoreChangedEvent& SourceEvent);

	static EBlueprintHelperReviewPendingLoadMode ResolveLoadMode(
		const FBlueprintHelperReviewStoreChangedEvent& NormalizedEvent);

	static FBlueprintHelperReviewPendingLoadRequestApplication BuildRequestApplication(
		const FString& Reason,
		EBlueprintHelperReviewPendingLoadMode Mode,
		const FBlueprintHelperReviewStoreChangedEvent& SourceEvent,
		const FBlueprintHelperReviewPagedChangeModel& PagedChangeModel,
		int32 PageSize);

	static FBlueprintHelperReviewPendingLoadResultApplication ApplyResult(
		const FBlueprintHelperReviewPendingLoadResult& Result,
		FBlueprintHelperReviewPagedChangeModel& PagedChangeModel,
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& CurrentChangeItems,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& CurrentSelectedChange,
		const FString& LastVisibleChangeRefreshSignature,
		int32 PageSize);

	static FString BuildVisibleChangeRefreshSignature(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes);

private:
	static int32 FindChangeIndexById(
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
		const FString& ChangeId);

	static bool ContainsChangeId(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
		const FString& ChangeId);

	static FString ResolveRecommendedSelectedChangeId(
		const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges,
		const FString& PreviousSelectedChangeId,
		const FString& PreviousSelectedAssetPath,
		int32 PreviousSelectedIndex);
};
