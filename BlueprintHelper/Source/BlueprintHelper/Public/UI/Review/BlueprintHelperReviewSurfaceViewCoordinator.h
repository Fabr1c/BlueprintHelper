// BlueprintHelper Review surface view coordinator.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewSurfaceView.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceViewCoordinator
{
public:
	void Reset();
	void RegisterSurface(const TSharedRef<FBlueprintHelperReviewSurfaceView>& SurfaceView);
	bool RefreshOverlay(EBlueprintHelperReviewSurface Surface) const;
	bool RefreshRows(EBlueprintHelperReviewSurface Surface) const;
	bool RefreshAllOverlays() const;

private:
	TSharedPtr<FBlueprintHelperReviewSurfaceView> FindSurfaceView(EBlueprintHelperReviewSurface Surface) const;

	TArray<TSharedRef<FBlueprintHelperReviewSurfaceView>> SurfaceViews;
};
