// BlueprintHelper Review surface diff frame presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceDiffFrameRoute
{
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	FBlueprintHelperReviewSurfaceChangePredicate ShouldShowChange = nullptr;
	bool bShouldBuildOverlay = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceDiffFramePresenter
{
public:
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveStructurePanelRoute(
		EBlueprintHelperReviewAssetKind AssetKind);
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveMainWorkspaceRoute(
		EBlueprintHelperReviewAssetKind AssetKind);
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveMyBlueprintRoute();
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveDetailsRoute();
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveSurfaceRoute(
		EBlueprintHelperReviewSurface Surface);

private:
	static FBlueprintHelperReviewSurfaceDiffFrameRoute ResolveSurfaceRoute(
		EBlueprintHelperReviewSurface Surface,
		bool bShouldBuildOverlay);
};
