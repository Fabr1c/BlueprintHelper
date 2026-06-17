// BlueprintHelper Review restore adapter interface.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewRestoreResult
{
	bool bSucceeded = false;
	EBlueprintHelperReviewChangeStatus NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	FString RollbackMode;
	FString Message;
	bool bSupersededDataCompactionEligible = false;
};

class BLUEPRINTHELPER_API IBlueprintHelperReviewRestoreAdapter
{
public:
	virtual ~IBlueprintHelperReviewRestoreAdapter();

	virtual FString GetTargetKind() const = 0;
	virtual FBlueprintHelperReviewRestoreResult RestoreBeforeSnapshot(
		const FBlueprintHelperReviewVisibleChange& Change) const = 0;
};
