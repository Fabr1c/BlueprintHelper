// BlueprintHelper Review reject mutation presentation model.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewRejectMutationPresentation
{
	TArray<FString> DebugMessages;
	TArray<TSharedRef<FJsonObject>> DebugBundleEvents;

	bool bCancelWaitingForStoreRefresh = false;
	bool bSetPresenterError = false;
	EBlueprintHelperReviewChangeStatus PresenterErrorStatus =
		EBlueprintHelperReviewChangeStatus::Pending;
	FString PresenterErrorMessage;

	bool bRefreshAfterFailure = false;
	FString RefreshReason;

	bool bShowNotification = false;
	FString NotificationKey;
	FString NotificationText;
	EBlueprintHelperReviewActionNotificationState NotificationState =
		EBlueprintHelperReviewActionNotificationState::Fail;
	bool bNotificationExpires = true;
	bool bNotificationUsesThrobber = false;

	bool bHasBatchResult = false;
	bool bBatchSucceeded = false;

	FString FeedbackStage;
	FString FeedbackDetail;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewRejectMutationPresenter
{
public:
	static FBlueprintHelperReviewRejectMutationPresentation BuildMissingChange(
		const FString& ChangeId,
		bool bIsBatchChange);

	static FBlueprintHelperReviewRejectMutationPresentation BuildLifecycleRootResult(
		const FString& ChangeId,
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewCascadeActionResult& Result,
		const FString& NotificationLabel,
		bool bIsBatchChange);

	static FBlueprintHelperReviewRejectMutationPresentation BuildSingleResult(
		const FString& ChangeId,
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewActionResult& Result,
		const FString& NotificationLabel,
		bool bIsBatchChange,
		const FString& DebugBundleSessionId,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange);

private:
	static FString BuildFriendlyActionMessage(
		const FString& Prefix,
		const FString& Detail);
	static FString BuildRejectNotificationKey(const FString& ChangeId);
};
