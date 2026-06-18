// BlueprintHelper Review surface host coordinator implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceHostCoordinator.h"

void FBlueprintHelperReviewSurfaceHostCoordinator::Configure(
	FBlueprintHelperReviewSurfaceViewCoordinator& SurfaceViewCoordinator,
	FBlueprintHelperReviewSurfaceHostCoordinatorDelegates Delegates) const
{
	SurfaceViewCoordinator.Reset();

	for (const FBlueprintHelperReviewSurfaceHostBinding& HostBinding : Delegates.HostBindings)
	{
		if (HostBinding.Surface == EBlueprintHelperReviewSurface::Unknown)
		{
			continue;
		}

		TFunction<bool()> OverlayRefresh;
		if (HostBinding.bSupportsOverlayRefresh)
		{
			if (TFunction<bool()>* OverlayHandler = Delegates.OverlayRefreshHandlers.Find(HostBinding.HostSlot))
			{
				OverlayRefresh = *OverlayHandler;
			}
		}

		TFunction<bool()> RowsRefresh;
		if (HostBinding.bSupportsRowRefresh)
		{
			if (TFunction<bool()>* RowsHandler = Delegates.RowRefreshHandlers.Find(HostBinding.Surface))
			{
				RowsRefresh = MoveTemp(*RowsHandler);
			}
		}

		RegisterSurface(
			SurfaceViewCoordinator,
			HostBinding.Surface,
			MoveTemp(OverlayRefresh),
			MoveTemp(RowsRefresh));
	}
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
