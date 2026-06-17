// BlueprintHelper Review reject mutation presentation application service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"

class FJsonObject;
struct FBlueprintHelperReviewRejectMutationPresentation;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewRejectMutationApplicationCallbacks
{
	TFunction<void()> CancelWaitingForStoreRefresh;
	TFunction<void(EBlueprintHelperReviewChangeStatus, const FString&)> SetPresenterError;
	TFunction<void(const FString&)> RefreshAfterFailure;
	TFunction<void(const FString&)> AddDebugMessage;
	TFunction<void(const TSharedRef<FJsonObject>&)> AppendDebugBundleEvent;
	TFunction<void(
		const FString&,
		const FString&,
		EBlueprintHelperReviewActionNotificationState,
		bool,
		bool)> ShowNotification;
	TFunction<void(const FString&, const FString&)> RecordFeedbackStage;
	TFunction<void(bool)> RecordBatchResult;
	TFunction<void()> RecordFinishedStage;
	TFunction<void()> ClearTransientActionState;
	TFunction<void()> FinishWorkflow;
	TFunction<bool()> IsWaitingForStoreRefresh;
	TFunction<void()> CompleteTiming;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewRejectMutationApplicationService
{
public:
	static void ApplyPresentation(
		const FBlueprintHelperReviewRejectMutationPresentation& Presentation,
		bool bHasChangeItem,
		const FBlueprintHelperReviewRejectMutationApplicationCallbacks& Callbacks);

	static void FinishMutation(
		const FBlueprintHelperReviewRejectMutationApplicationCallbacks& Callbacks);
};
