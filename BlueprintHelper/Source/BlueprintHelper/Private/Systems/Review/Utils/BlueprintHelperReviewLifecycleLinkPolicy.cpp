#include "Shared/Review/BlueprintHelperReviewLifecycleLinkPolicy.h"

#include "Shared/Review/BlueprintHelperReviewTargetMatcher.h"

bool FBlueprintHelperReviewLifecycleLinkPolicy::CanLinkAsChild(
	const FBlueprintHelperReviewBoundaryModel& Parent,
	const FBlueprintHelperReviewBoundaryModel& Child)
{
	return FBlueprintHelperReviewTargetMatcher::BelongsToLifecycleRoot(Child, Parent);
}

bool FBlueprintHelperReviewLifecycleLinkPolicy::CanCascadeReject(
	const FBlueprintHelperReviewBoundaryModel& Parent,
	const FBlueprintHelperReviewBoundaryModel& Child)
{
	return Parent.bRejectRemovesChildren && CanLinkAsChild(Parent, Child);
}
