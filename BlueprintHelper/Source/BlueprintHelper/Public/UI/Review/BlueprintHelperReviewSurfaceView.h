// BlueprintHelper Review independent surface view.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceView
{
public:
	explicit FBlueprintHelperReviewSurfaceView(EBlueprintHelperReviewSurface InSurface);

	EBlueprintHelperReviewSurface GetSurface() const;
	void SetOverlayRefresh(TFunction<bool()> InRefreshOverlay);
	void SetRowsRefresh(TFunction<bool()> InRefreshRows);
	bool RefreshOverlay() const;
	bool RefreshRows() const;

private:
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	TFunction<bool()> RefreshOverlayDelegate;
	TFunction<bool()> RefreshRowsDelegate;
};
