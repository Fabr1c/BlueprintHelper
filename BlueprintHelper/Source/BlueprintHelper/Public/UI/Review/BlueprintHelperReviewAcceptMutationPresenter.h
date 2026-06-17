// BlueprintHelper Review accept mutation presentation model.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewAcceptMutationPresentation
{
	TArray<FString> DebugMessages;

	bool bSetPresenterError = false;
	EBlueprintHelperReviewChangeStatus PresenterErrorStatus =
		EBlueprintHelperReviewChangeStatus::Pending;
	FString PresenterErrorMessage;

	bool bShowNotification = false;
	FString NotificationKey;
	FString NotificationText;
	EBlueprintHelperReviewActionNotificationState NotificationState =
		EBlueprintHelperReviewActionNotificationState::Fail;
	bool bNotificationExpires = true;
	bool bNotificationUsesThrobber = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewAcceptMutationPresenter
{
public:
	static FBlueprintHelperReviewAcceptMutationPresentation BuildResult(
		const FString& ChangeId,
		const FBlueprintHelperReviewActionResult& Result,
		const FString& NotificationLabel);

private:
	static FString BuildAcceptNotificationKey(const FString& ChangeId);
};
