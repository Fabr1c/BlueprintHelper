#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewRestoreAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperClassDefaultSetterRestoreAdapter
	: public IBlueprintHelperReviewRestoreAdapter
{
public:
	virtual FString GetTargetKind() const override;
	virtual FBlueprintHelperReviewRestoreResult RestoreBeforeSnapshot(
		const FBlueprintHelperReviewVisibleChange& Change) const override;
};
