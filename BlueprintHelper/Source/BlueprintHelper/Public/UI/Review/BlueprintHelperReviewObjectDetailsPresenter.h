// BlueprintHelper Review object details presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SWidget.h"

class SKismetInspector;
struct FBlueprintHelperReviewPanelSurfacePresenterArgs;
struct FBlueprintHelperReviewAssetContext;

class BLUEPRINTHELPER_API FBlueprintHelperReviewObjectDetailsPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		TSharedPtr<SKismetInspector>& OutKismetInspector);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};
