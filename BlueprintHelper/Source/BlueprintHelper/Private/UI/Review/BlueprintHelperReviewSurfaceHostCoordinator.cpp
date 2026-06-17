// BlueprintHelper Review surface host coordinator implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceHostCoordinator.h"

void FBlueprintHelperReviewSurfaceHostCoordinator::Configure(
	FBlueprintHelperReviewSurfaceViewCoordinator& SurfaceViewCoordinator,
	FBlueprintHelperReviewSurfaceHostCoordinatorDelegates Delegates) const
{
	SurfaceViewCoordinator.Reset();

	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::Components,
		Delegates.StructureOverlayRefresh,
		MoveTemp(Delegates.ComponentsRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		Delegates.StructureOverlayRefresh,
		MoveTemp(Delegates.WidgetTreeRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::MyBlueprint,
		MoveTemp(Delegates.MyBlueprintOverlayRefresh),
		MoveTemp(Delegates.MyBlueprintRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::Details,
		MoveTemp(Delegates.DetailsOverlayRefresh),
		MoveTemp(Delegates.DetailsRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::DataTable,
		Delegates.MainWorkspaceOverlayRefresh,
		MoveTemp(Delegates.DataTableRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::DataAsset,
		Delegates.MainWorkspaceOverlayRefresh,
		MoveTemp(Delegates.DataAssetRowsRefresh));
	RegisterSurface(
		SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface::Material,
		Delegates.MainWorkspaceOverlayRefresh,
		MoveTemp(Delegates.MaterialRowsRefresh));
}

void FBlueprintHelperReviewSurfaceHostCoordinator::RegisterSurface(
	FBlueprintHelperReviewSurfaceViewCoordinator& SurfaceViewCoordinator,
	EBlueprintHelperReviewSurface Surface,
	TFunction<bool()> OverlayRefresh,
	TFunction<bool()> RowsRefresh)
{
	TSharedRef<FBlueprintHelperReviewSurfaceView> SurfaceView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(Surface);
	SurfaceView->SetOverlayRefresh(MoveTemp(OverlayRefresh));
	SurfaceView->SetRowsRefresh(MoveTemp(RowsRefresh));
	SurfaceViewCoordinator.RegisterSurface(SurfaceView);
}
