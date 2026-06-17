// BlueprintHelper Review accept mutation presentation application service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"

struct FBlueprintHelperReviewAcceptMutationPresentation;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewAcceptMutationApplicationCallbacks
{
	TFunction<void(EBlueprintHelperReviewChangeStatus, const FString&)> SetPresenterError;
	TFunction<void(const FString&)> AddDebugMessage;
	TFunction<void(
		const FString&,
		const FString&,
		EBlueprintHelperReviewActionNotificationState,
		bool,
		bool)> ShowNotification;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewAcceptMutationApplicationService
{
public:
	static void ApplyPresentation(
		const FBlueprintHelperReviewAcceptMutationPresentation& Presentation,
		const FBlueprintHelperReviewAcceptMutationApplicationCallbacks& Callbacks);
};
