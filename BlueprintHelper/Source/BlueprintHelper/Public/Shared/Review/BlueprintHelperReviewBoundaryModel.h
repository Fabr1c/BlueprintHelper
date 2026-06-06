#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewBoundaryModel
{
	FString AssetKey;
	FString LocationKey;
	FString TargetKey;
	FString TargetKind;
	FString TargetSubKind;
	FString ScopeIdentity;
	FString LifecycleObjectKey;
	FString LifecycleParentKey;
	FString VisualGroupKey;
	bool bIsAssetLifecycleRoot = false;
	bool bIsObjectLifecycleRoot = false;
	bool bRejectRemovesChildren = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewBoundaryModelBuilder
{
public:
	static FBlueprintHelperReviewBoundaryModel FromAtomicTarget(
		const FBlueprintHelperReviewAtomicTarget& Target);
	static FBlueprintHelperReviewBoundaryModel FromVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change);
};
