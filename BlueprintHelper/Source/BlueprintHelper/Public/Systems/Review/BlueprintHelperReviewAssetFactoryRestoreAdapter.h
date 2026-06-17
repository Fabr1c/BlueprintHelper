// BlueprintHelper Review asset factory restore adapter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewRestoreAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewAssetFactoryRestoreAdapter : public IBlueprintHelperReviewRestoreAdapter
{
public:
	virtual FString GetTargetKind() const override;
	virtual FBlueprintHelperReviewRestoreResult RestoreBeforeSnapshot(
		const FBlueprintHelperReviewVisibleChange& Change) const override;
};
