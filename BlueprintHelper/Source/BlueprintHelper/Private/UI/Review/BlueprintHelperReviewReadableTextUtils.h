// BlueprintHelper Review readable text utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperReviewReadableTextUtils
{
public:
	static FString GetReviewListTargetText(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		EBlueprintHelperReviewSurface Surface);

	static FString BuildReadableChangeTitle(const FBlueprintHelperReviewVisibleChange& Change);

private:
	static FString ExtractReadableTail(FString Text);
	static FString ExtractAssetShortNameFromPath(FString AssetPath);
	static FString StripEncodedPackagePrefix(FString Text);
	static bool IsAssetFactoryChange(const FBlueprintHelperReviewVisibleChange& Change);
	static FString GetAssetFactoryReadableName(const FBlueprintHelperReviewVisibleChange& Change);
	static FString GetAssetFactoryReadableSuffix(const FBlueprintHelperReviewVisibleChange& Change);
	static FString GetReadableTargetName(const FBlueprintHelperReviewVisibleChange& Change);
	static FString GetReadableTargetSuffix(const FBlueprintHelperReviewVisibleChange& Change);
	static FString GetReadableChangeVerb(EBlueprintHelperReviewChangeKind ChangeKind);
};
