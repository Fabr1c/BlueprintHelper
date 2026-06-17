// BlueprintHelper Review accept mutation presentation model implementation.

#include "UI/Review/BlueprintHelperReviewAcceptMutationPresenter.h"

FBlueprintHelperReviewAcceptMutationPresentation
FBlueprintHelperReviewAcceptMutationPresenter::BuildResult(
	const FString& ChangeId,
	const FBlueprintHelperReviewActionResult& Result,
	const FString& NotificationLabel)
{
	FBlueprintHelperReviewAcceptMutationPresentation Presentation;
	if (Result.bSucceeded)
	{
		Presentation.DebugMessages.Add(FString::Printf(
			TEXT("Accept change id=%s result=waiting_for_store_refresh"),
			*ChangeId));
	}
	else
	{
		Presentation.bSetPresenterError = true;
		Presentation.PresenterErrorStatus = Result.NewStatus;
		Presentation.PresenterErrorMessage = Result.Message;
	}

	Presentation.DebugMessages.Add(FString::Printf(
		TEXT("Accept change id=%s success=%d message=\"%s\""),
		*ChangeId,
		Result.bSucceeded ? 1 : 0,
		*Result.Message));

	Presentation.bShowNotification = true;
	Presentation.NotificationKey = BuildAcceptNotificationKey(ChangeId);
	Presentation.NotificationText = FString::Printf(
		TEXT("%s: %s"),
		Result.bSucceeded ? TEXT("Accepted") : TEXT("Accept failed"),
		*NotificationLabel);
	Presentation.NotificationState = Result.bSucceeded
		? EBlueprintHelperReviewActionNotificationState::Success
		: EBlueprintHelperReviewActionNotificationState::Fail;
	return Presentation;
}

FString FBlueprintHelperReviewAcceptMutationPresenter::BuildAcceptNotificationKey(
	const FString& ChangeId)
{
	return TEXT("accept:") + ChangeId;
}
