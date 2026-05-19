// BlueprintHelper Review independent surface view implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceView.h"

FBlueprintHelperReviewSurfaceView::FBlueprintHelperReviewSurfaceView(
	EBlueprintHelperReviewSurface InSurface)
	: Surface(InSurface)
{
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewSurfaceView::GetSurface() const
{
	return Surface;
}

void FBlueprintHelperReviewSurfaceView::SetOverlayRefresh(TFunction<bool()> InRefreshOverlay)
{
	RefreshOverlayDelegate = MoveTemp(InRefreshOverlay);
}

void FBlueprintHelperReviewSurfaceView::SetRowsRefresh(TFunction<bool()> InRefreshRows)
{
	RefreshRowsDelegate = MoveTemp(InRefreshRows);
}

bool FBlueprintHelperReviewSurfaceView::RefreshOverlay() const
{
	return RefreshOverlayDelegate ? RefreshOverlayDelegate() : false;
}

bool FBlueprintHelperReviewSurfaceView::RefreshRows() const
{
	return RefreshRowsDelegate ? RefreshRowsDelegate() : false;
}
