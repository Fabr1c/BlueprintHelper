// BlueprintHelper Review action service.

#pragma once

#include "CoreMinimal.h"
#include "Structure/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewActionResult
{
	bool bSucceeded = false;
	FString TargetTransactionId;
	EBlueprintHelperReviewChangeStatus NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	FString RollbackMode;
	FString Message;
	bool bSupersededDataCompactionEligible = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewActionService
{
public:
	FBlueprintHelperReviewActionResult AcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;

	FBlueprintHelperReviewActionResult RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;
};
