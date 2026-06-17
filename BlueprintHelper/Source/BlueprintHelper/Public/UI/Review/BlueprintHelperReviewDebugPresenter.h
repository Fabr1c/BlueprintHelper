// BlueprintHelper Review debug presenter.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewDebugEventModel.h"

class FBlueprintHelperReviewDebugProjectionRegistry;

class BLUEPRINTHELPER_API FBlueprintHelperReviewDebugPresenter
{
public:
	FBlueprintHelperReviewDebugPresenter();
	explicit FBlueprintHelperReviewDebugPresenter(
		TSharedPtr<FBlueprintHelperReviewDebugProjectionRegistry> InProjectionRegistry);

	bool LoadBundle(
		const FString& BundlePath,
		FBlueprintHelperReviewDebugTimelineModel& OutTimeline,
		FString& OutError) const;

private:
	TSharedPtr<FBlueprintHelperReviewDebugProjectionRegistry> ProjectionRegistry;
};
