#include "Shared/Review/BlueprintHelperReviewTargetMatcher.h"

bool FBlueprintHelperReviewTargetMatcher::MatchesTargetKey(
	const FBlueprintHelperReviewBoundaryModel& Boundary,
	const FString& TargetKey)
{
	return !Boundary.TargetKey.IsEmpty()
		&& !TargetKey.IsEmpty()
		&& Boundary.TargetKey.Equals(TargetKey, ESearchCase::IgnoreCase);
}

bool FBlueprintHelperReviewTargetMatcher::MatchesAnyTargetKey(
	const FBlueprintHelperReviewBoundaryModel& Boundary,
	const TArray<FString>& TargetKeys)
{
	for (const FString& TargetKey : TargetKeys)
	{
		if (MatchesTargetKey(Boundary, TargetKey))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewTargetMatcher::MatchesScopeIdentity(
	const FBlueprintHelperReviewBoundaryModel& Boundary,
	const FString& ScopeIdentity)
{
	return !Boundary.ScopeIdentity.IsEmpty()
		&& !ScopeIdentity.IsEmpty()
		&& Boundary.ScopeIdentity.Equals(ScopeIdentity, ESearchCase::IgnoreCase);
}

bool FBlueprintHelperReviewTargetMatcher::BelongsToLifecycleRoot(
	const FBlueprintHelperReviewBoundaryModel& Child,
	const FBlueprintHelperReviewBoundaryModel& Root)
{
	if ((!Root.bIsAssetLifecycleRoot && !Root.bIsObjectLifecycleRoot)
		|| Root.AssetKey.IsEmpty()
		|| Child.AssetKey.IsEmpty()
		|| !Root.AssetKey.Equals(Child.AssetKey, ESearchCase::IgnoreCase)
		|| MatchesTargetKey(Child, Root.TargetKey))
	{
		return false;
	}

	if (Root.bIsAssetLifecycleRoot)
	{
		return true;
	}

	if (Child.LifecycleParentKey.IsEmpty())
	{
		return false;
	}

	return (!Root.LifecycleObjectKey.IsEmpty()
			&& Child.LifecycleParentKey.Equals(Root.LifecycleObjectKey, ESearchCase::IgnoreCase))
		|| (!Root.TargetKey.IsEmpty()
			&& Child.LifecycleParentKey.Equals(Root.TargetKey, ESearchCase::IgnoreCase));
}
