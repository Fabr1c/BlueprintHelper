#include "Shared/Review/BlueprintHelperReviewBoundaryModel.h"

FBlueprintHelperReviewBoundaryModel FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	FBlueprintHelperReviewBoundaryModel Boundary;
	Boundary.AssetKey = Target.AssetPath;
	Boundary.LocationKey = Target.GraphName;
	Boundary.TargetKey = Target.TargetKey;
	Boundary.TargetKind = Target.TargetKind;
	Boundary.TargetSubKind = Target.TargetSubKind;
	Boundary.ScopeIdentity = Target.ScopeIdentity;
	Boundary.LifecycleObjectKey = Target.LifecycleObjectKey;
	Boundary.LifecycleParentKey = Target.LifecycleParentKey;
	Boundary.VisualGroupKey = Target.VisualGroupKey;
	Boundary.bIsAssetLifecycleRoot = Target.TargetKind.Equals(TEXT("asset"), ESearchCase::IgnoreCase) &&
		Target.LifecycleObjectKey.Equals(TEXT("asset:asset"), ESearchCase::IgnoreCase);
	Boundary.bIsObjectLifecycleRoot = !Target.LifecycleObjectKey.IsEmpty() && Target.LifecycleParentKey.IsEmpty();
	return Boundary;
}

FBlueprintHelperReviewBoundaryModel FBlueprintHelperReviewBoundaryModelBuilder::FromVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	FBlueprintHelperReviewBoundaryModel Boundary;
	if (Change.AtomicTargets.Num() > 0)
	{
		Boundary = FromAtomicTarget(Change.AtomicTargets[0]);
	}

	Boundary.AssetKey = Boundary.AssetKey.IsEmpty() ? Change.AssetPath : Boundary.AssetKey;
	Boundary.LocationKey = !Change.LocationKey.IsEmpty() ? Change.LocationKey : Boundary.LocationKey;
	if (Boundary.LocationKey.IsEmpty())
	{
		Boundary.LocationKey = Change.GraphName;
	}
	Boundary.ScopeIdentity = !Boundary.ScopeIdentity.IsEmpty() ? Boundary.ScopeIdentity : Change.ScopeIdentity;
	if (Boundary.LifecycleParentKey.IsEmpty() && !Change.ParentChangeId.IsEmpty())
	{
		Boundary.LifecycleParentKey = Change.ParentChangeId;
	}
	Boundary.bIsAssetLifecycleRoot = Change.bIsAssetLifecycleRoot;
	Boundary.bIsObjectLifecycleRoot =
		Change.bIsObjectLifecycleRoot ||
		Change.bIsAssetLifecycleRoot ||
		(!Boundary.LifecycleObjectKey.IsEmpty() && Boundary.LifecycleParentKey.IsEmpty());
	Boundary.bRejectRemovesChildren = Change.bRejectRemovesChildren;
	return Boundary;
}
