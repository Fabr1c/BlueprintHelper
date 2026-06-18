// BlueprintHelper Review surface diff frame presenter implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceDiffFramePresenter.h"

#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveStructurePanelRoute(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	return ResolveSurfaceRoute(
		FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(AssetKind),
		true);
}

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	const EBlueprintHelperReviewSurface Surface =
		FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(AssetKind);
	return ResolveSurfaceRoute(
		Surface,
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(Surface));
}

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMyBlueprintRoute()
{
	return ResolveSurfaceRoute(EBlueprintHelperReviewSurface::MyBlueprint, true);
}

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveDetailsRoute()
{
	return ResolveSurfaceRoute(EBlueprintHelperReviewSurface::Details, true);
}

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveSurfaceRoute(
	EBlueprintHelperReviewSurface Surface)
{
	return ResolveSurfaceRoute(Surface, true);
}

FBlueprintHelperReviewSurfaceDiffFrameRoute
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveSurfaceRoute(
	EBlueprintHelperReviewSurface Surface,
	bool bShouldBuildOverlay)
{
	const TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault();
	FBlueprintHelperReviewSurfaceDiffFrameRoute Route;
	Route.Surface = Surface;
	Route.ShouldShowChange = Registry->FindPredicate(Surface);
	Route.bShouldBuildOverlay = bShouldBuildOverlay
		&& Route.ShouldShowChange != nullptr
		&& Registry->CanBuildOverlay(Surface);
	return Route;
}
