// BlueprintHelper Review target identity implementation.

#include "Systems/Review/BlueprintHelperReviewTargetIdentity.h"

FString FBlueprintHelperReviewTargetIdentity::ToStableKey() const
{
	TArray<FString> Parts;
	auto AddPart = [&Parts](const TCHAR* Key, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Parts.Add(FString::Printf(TEXT("%s=%s"), Key, *Value));
		}
	};

	AddPart(TEXT("asset"), AssetPath);
	AddPart(TEXT("asset_kind"), AssetKind);
	AddPart(TEXT("surface"), SurfaceKind);
	AddPart(TEXT("target_kind"), TargetKind);
	AddPart(TEXT("target_key"), TargetKey);
	AddPart(TEXT("parent"), ParentTargetKey);
	return FString::Join(Parts, TEXT("|"));
}

FBlueprintHelperReviewTargetIdentity FBlueprintHelperReviewTargetIdentity::FromAtomicTarget(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	FBlueprintHelperReviewTargetIdentity Identity;
	Identity.AssetPath = Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath;
	Identity.AssetKind = Target.TargetKind.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase)
		? Target.TargetSubKind
		: FString();
	Identity.SurfaceKind = BlueprintHelperReviewSurfaceToString(Target.Surface);
	Identity.TargetKind = Target.TargetKind;
	Identity.TargetKey = Target.TargetKey;
	Identity.ParentTargetKey = Target.LifecycleParentKey;
	return Identity;
}
