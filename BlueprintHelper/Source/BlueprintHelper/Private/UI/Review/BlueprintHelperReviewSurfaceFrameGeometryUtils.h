// BlueprintHelper Review surface frame geometry utilities.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class FBlueprintHelperReviewSurfaceFrameGeometryUtils
{
public:
	static void ApplyRowGeometryPadding(FBlueprintHelperReviewSurfaceGeometryAnchor& Anchor);

	static bool TryResolveSlateRowGeometry(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetText,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);

	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
};
