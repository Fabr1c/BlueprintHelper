// BlueprintHelper Review generic surface projection adapter implementation.

#include "UI/Review/BlueprintHelperReviewGenericSurfaceProjectionAdapter.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"

FBlueprintHelperReviewGenericSurfaceProjectionAdapter::FBlueprintHelperReviewGenericSurfaceProjectionAdapter(
	const FString& InAssetKind,
	const FString& InSurfaceKind,
	const FString& InTargetKind)
	: AssetKind(InAssetKind)
	, SurfaceKind(InSurfaceKind)
	, TargetKind(InTargetKind)
{
}

FString FBlueprintHelperReviewGenericSurfaceProjectionAdapter::GetAssetKind() const
{
	return AssetKind;
}

FString FBlueprintHelperReviewGenericSurfaceProjectionAdapter::GetSurfaceKind() const
{
	return SurfaceKind;
}

FString FBlueprintHelperReviewGenericSurfaceProjectionAdapter::GetTargetKind() const
{
	return TargetKind;
}

bool FBlueprintHelperReviewGenericSurfaceProjectionAdapter::CanProject(
	const FBlueprintHelperReviewTargetIdentity& Identity) const
{
	return (AssetKind.IsEmpty() || Identity.AssetKind.Equals(AssetKind, ESearchCase::IgnoreCase))
		&& Identity.SurfaceKind.Equals(SurfaceKind, ESearchCase::IgnoreCase)
		&& Identity.TargetKind.Equals(TargetKind, ESearchCase::IgnoreCase);
}

FBlueprintHelperReviewSurfaceProjectionResult FBlueprintHelperReviewGenericSurfaceProjectionAdapter::Project(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewSurfaceProjectionResult Result;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (!FBlueprintHelperReviewStatusUtils::IsOpenReviewStatus(Target.Status))
		{
			continue;
		}

		const FBlueprintHelperReviewTargetIdentity Identity =
			FBlueprintHelperReviewTargetIdentity::FromAtomicTarget(Change, Target);
		FBlueprintHelperReviewTargetIdentity AdapterIdentity = Identity;
		if (AdapterIdentity.AssetKind.IsEmpty())
		{
			AdapterIdentity.AssetKind = AssetKind;
		}
		if (!CanProject(AdapterIdentity))
		{
			continue;
		}

		FBlueprintHelperReviewSurfaceDiffProjectionModel Model;
		Model.ReviewEventId = Change.ChangeId;
		Model.AssetPath = Identity.AssetPath;
		Model.SurfaceKind = SurfaceKind;
		Model.TargetKind = Target.TargetKind;
		Model.TargetKey = Target.TargetKey;
		if (Model.TargetKey.IsEmpty())
		{
			Model.TargetKey = Target.PropertyPath;
		}
		if (Model.TargetKey.IsEmpty())
		{
			Model.TargetKey = Target.ComponentPath;
		}
		if (Model.TargetKey.IsEmpty())
		{
			Model.TargetKey = Target.DisplayLabel;
		}
		if (Model.TargetKey.IsEmpty())
		{
			Model.TargetKey = Change.LocationKey;
		}
		if (Model.TargetKey.IsEmpty())
		{
			Model.TargetKey = Change.DisplayLabel;
		}
		if (!Target.TargetKey.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.TargetKey);
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.PropertyPath);
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.ComponentPath);
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.DisplayLabel);
		}
		if (!Change.LocationKey.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Change.LocationKey);
		}
		if (!Change.DisplayLabel.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Change.DisplayLabel);
		}
		if (!Model.TargetKey.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Model.TargetKey);
		}
		Model.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Change.DisplayLabel : Target.DisplayLabel;
		Model.DiffColor = BlueprintHelperReviewSurfaceDiffColor(Change.ChangeKind);
		Model.ChangeKind = Change.ChangeKind;
		Model.bCanAccept = Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
		Model.bCanReject = Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
		Result.DiffModels.Add(Model);
	}

	Result.bProjected = Result.DiffModels.Num() > 0;
	Result.Message = Result.bProjected ? TEXT("projected") : TEXT("no_matching_targets");
	return Result;
}
