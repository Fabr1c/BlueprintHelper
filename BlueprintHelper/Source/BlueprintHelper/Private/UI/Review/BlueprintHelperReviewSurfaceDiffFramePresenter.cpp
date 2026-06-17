// BlueprintHelper Review surface diff frame presenter implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceDiffFramePresenter.h"

#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataAssetPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataTablePresenter.h"
#include "UI/Review/BlueprintHelperReviewGraphPresenter.h"
#include "UI/Review/BlueprintHelperReviewMaterialPresenter.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "UI/Review/BlueprintHelperReviewObjectDetailsPresenter.h"
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
	FBlueprintHelperReviewSurfaceDiffFrameRoute Route;
	Route.Surface = Surface;
	Route.ShouldShowChange = ResolvePredicate(Surface);
	Route.bShouldBuildOverlay = bShouldBuildOverlay && Route.ShouldShowChange != nullptr;
	return Route;
}

FBlueprintHelperReviewSurfaceChangePredicate
FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolvePredicate(
	EBlueprintHelperReviewSurface Surface)
{
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Graph:
		return &FBlueprintHelperReviewGraphPresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::Components:
		return &FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		return &FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::MyBlueprint:
		return &FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::Details:
		return &FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::DataTable:
		return &FBlueprintHelperReviewDataTablePresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::DataAsset:
		return &FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange;
	case EBlueprintHelperReviewSurface::Material:
		return &FBlueprintHelperReviewMaterialPresenter::ShouldShowChange;
	default:
		return nullptr;
	}
}
