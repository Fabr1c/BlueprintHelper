// BlueprintHelper Review accept mutation presentation/application service tests.

#include "UI/Review/BlueprintHelperReviewAcceptMutationApplicationService.h"

#include "Misc/AutomationTest.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "UI/Review/BlueprintHelperReviewAcceptMutationPresenter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptMutationPresenter_BuildsSuccessPresentation,
	"BlueprintHelper.Review.Panel.AcceptMutationPresenter.BuildsSuccessPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewAcceptMutationPresenter_BuildsSuccessPresentation::RunTest(const FString&)
{
	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = true;
	Result.Message = TEXT("accepted");

	const FBlueprintHelperReviewAcceptMutationPresentation Presentation =
		FBlueprintHelperReviewAcceptMutationPresenter::BuildResult(
			TEXT("change_1"),
			Result,
			TEXT("Display Label"));

	TestFalse(TEXT("success does not set presenter error"), Presentation.bSetPresenterError);
	TestTrue(TEXT("success notification"), Presentation.bShowNotification);
	TestEqual(TEXT("success notification state"), static_cast<int32>(Presentation.NotificationState), static_cast<int32>(EBlueprintHelperReviewActionNotificationState::Success));
	TestTrue(TEXT("success debug message"), Presentation.DebugMessages.Contains(TEXT("Accept change id=change_1 result=waiting_for_store_refresh")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptMutationApplicationService_AppliesPresentationCallbacks,
	"BlueprintHelper.Review.Panel.AcceptMutationApplicationService.AppliesPresentationCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewAcceptMutationApplicationService_AppliesPresentationCallbacks::RunTest(const FString&)
{
	FBlueprintHelperReviewAcceptMutationPresentation Presentation;
	Presentation.bSetPresenterError = true;
	Presentation.PresenterErrorStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Presentation.PresenterErrorMessage = TEXT("accept_failed");
	Presentation.DebugMessages.Add(TEXT("debug_message"));
	Presentation.bShowNotification = true;
	Presentation.NotificationKey = TEXT("accept:change");
	Presentation.NotificationText = TEXT("Accept failed");
	Presentation.NotificationState = EBlueprintHelperReviewActionNotificationState::Fail;

	bool bPresenterErrorSet = false;
	int32 DebugMessageCount = 0;
	bool bNotificationShown = false;

	FBlueprintHelperReviewAcceptMutationApplicationCallbacks Callbacks;
	Callbacks.SetPresenterError = [&bPresenterErrorSet](
		EBlueprintHelperReviewChangeStatus,
		const FString&)
	{
		bPresenterErrorSet = true;
	};
	Callbacks.AddDebugMessage = [&DebugMessageCount](const FString&)
	{
		++DebugMessageCount;
	};
	Callbacks.ShowNotification = [&bNotificationShown](
		const FString&,
		const FString&,
		EBlueprintHelperReviewActionNotificationState,
		bool,
		bool)
	{
		bNotificationShown = true;
	};

	FBlueprintHelperReviewAcceptMutationApplicationService::ApplyPresentation(
		Presentation,
		Callbacks);

	TestTrue(TEXT("presenter error"), bPresenterErrorSet);
	TestEqual(TEXT("debug messages"), DebugMessageCount, 1);
	TestTrue(TEXT("notification"), bNotificationShown);
	return true;
}

#endif
