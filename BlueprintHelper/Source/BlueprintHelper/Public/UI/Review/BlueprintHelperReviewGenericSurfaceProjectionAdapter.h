// BlueprintHelper Review generic surface projection adapter.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewGenericSurfaceProjectionAdapter
	: public IBlueprintHelperReviewSurfaceProjectionAdapter
{
public:
	FBlueprintHelperReviewGenericSurfaceProjectionAdapter(
		const FString& InAssetKind,
		const FString& InSurfaceKind,
		const FString& InTargetKind);

	virtual FString GetAssetKind() const override;
	virtual FString GetSurfaceKind() const override;
	virtual FString GetTargetKind() const override;
	virtual bool CanProject(const FBlueprintHelperReviewTargetIdentity& Identity) const override;
	virtual FBlueprintHelperReviewSurfaceProjectionResult Project(
		const FBlueprintHelperReviewVisibleChange& Change) const override;

private:
	FString AssetKind;
	FString SurfaceKind;
	FString TargetKind;
};
