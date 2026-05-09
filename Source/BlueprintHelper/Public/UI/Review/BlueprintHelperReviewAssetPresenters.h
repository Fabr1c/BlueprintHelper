// BlueprintHelper Review asset-specific presenters.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;

class BLUEPRINTHELPER_API FBlueprintHelperReviewUMGWidgetTreePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(const FBlueprintHelperReviewAssetContext& Context);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDataTablePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(const FBlueprintHelperReviewAssetContext& Context);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(const FBlueprintHelperReviewAssetContext& Context);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};
