// BlueprintHelper Review surface view coordinator implementation.

#include "UI/Review/BlueprintHelperReviewSurfaceViewCoordinator.h"

void FBlueprintHelperReviewSurfaceViewCoordinator::Reset()
{
	SurfaceViews.Reset();
}

void FBlueprintHelperReviewSurfaceViewCoordinator::RegisterSurface(
	const TSharedRef<FBlueprintHelperReviewSurfaceView>& SurfaceView)
{
	if (SurfaceView->GetSurface() == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	for (TSharedRef<FBlueprintHelperReviewSurfaceView>& ExistingSurfaceView : SurfaceViews)
	{
		if (ExistingSurfaceView->GetSurface() == SurfaceView->GetSurface())
		{
			ExistingSurfaceView = SurfaceView;
			return;
		}
	}

	SurfaceViews.Add(SurfaceView);
}

bool FBlueprintHelperReviewSurfaceViewCoordinator::RefreshOverlay(
	EBlueprintHelperReviewSurface Surface) const
{
	const TSharedPtr<FBlueprintHelperReviewSurfaceView> SurfaceView = FindSurfaceView(Surface);
	if (!SurfaceView.IsValid())
	{
		return false;
	}
	return SurfaceView->RefreshOverlay();
}

bool FBlueprintHelperReviewSurfaceViewCoordinator::RefreshRows(
	EBlueprintHelperReviewSurface Surface) const
{
	const TSharedPtr<FBlueprintHelperReviewSurfaceView> SurfaceView = FindSurfaceView(Surface);
	if (!SurfaceView.IsValid())
	{
		return false;
	}
	return SurfaceView->RefreshRows();
}

bool FBlueprintHelperReviewSurfaceViewCoordinator::RefreshAllOverlays() const
{
	bool bHandled = false;
	for (const TSharedRef<FBlueprintHelperReviewSurfaceView>& SurfaceView : SurfaceViews)
	{
		if (SurfaceView->RefreshOverlay())
		{
			bHandled = true;
		}
	}
	return bHandled;
}

TSharedPtr<FBlueprintHelperReviewSurfaceView>
FBlueprintHelperReviewSurfaceViewCoordinator::FindSurfaceView(EBlueprintHelperReviewSurface Surface) const
{
	for (const TSharedRef<FBlueprintHelperReviewSurfaceView>& SurfaceView : SurfaceViews)
	{
		if (SurfaceView->GetSurface() == Surface)
		{
			return SurfaceView;
		}
	}
	return nullptr;
}
