// BlueprintHelper Review UMG widget tree presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenterTypes.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewAssetContext;
struct FBlueprintHelperReviewPanelSurfacePresenterArgs;

class BLUEPRINTHELPER_API FBlueprintHelperReviewUMGWidgetTreePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewWidgetTreePresenterState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

