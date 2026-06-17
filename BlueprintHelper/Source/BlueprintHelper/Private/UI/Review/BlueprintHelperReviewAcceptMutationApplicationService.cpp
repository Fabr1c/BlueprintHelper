// BlueprintHelper Review accept mutation presentation application service implementation.

#include "UI/Review/BlueprintHelperReviewAcceptMutationApplicationService.h"

#include "UI/Review/BlueprintHelperReviewAcceptMutationPresenter.h"

void FBlueprintHelperReviewAcceptMutationApplicationService::ApplyPresentation(
	const FBlueprintHelperReviewAcceptMutationPresentation& Presentation,
	const FBlueprintHelperReviewAcceptMutationApplicationCallbacks& Callbacks)
{
	if (Presentation.bSetPresenterError && Callbacks.SetPresenterError)
	{
		Callbacks.SetPresenterError(
			Presentation.PresenterErrorStatus,
			Presentation.PresenterErrorMessage);
	}

	if (Callbacks.AddDebugMessage)
	{
		for (const FString& DebugMessage : Presentation.DebugMessages)
		{
			Callbacks.AddDebugMessage(DebugMessage);
		}
	}

	if (Presentation.bShowNotification && Callbacks.ShowNotification)
	{
		Callbacks.ShowNotification(
			Presentation.NotificationKey,
			Presentation.NotificationText,
			Presentation.NotificationState,
			Presentation.bNotificationExpires,
			Presentation.bNotificationUsesThrobber);
	}
}
