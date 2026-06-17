// BlueprintHelper Review target identity.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewTargetIdentity
{
	FString AssetPath;
	FString AssetKind;
	FString SurfaceKind;
	FString TargetKind;
	FString TargetKey;
	FString ParentTargetKey;

	FString ToStableKey() const;

	static FBlueprintHelperReviewTargetIdentity FromAtomicTarget(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewAtomicTarget& Target);
};
