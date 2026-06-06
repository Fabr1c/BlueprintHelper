#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewBoundaryModel.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewLifecycleLinkPolicy
{
public:
	static bool CanLinkAsChild(
		const FBlueprintHelperReviewBoundaryModel& Parent,
		const FBlueprintHelperReviewBoundaryModel& Child);
	static bool CanCascadeReject(
		const FBlueprintHelperReviewBoundaryModel& Parent,
		const FBlueprintHelperReviewBoundaryModel& Child);
};
