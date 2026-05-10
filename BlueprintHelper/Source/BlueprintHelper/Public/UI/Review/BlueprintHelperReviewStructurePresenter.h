// BlueprintHelper Review Structure presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenterTypes.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewAssetContext;

class BLUEPRINTHELPER_API FBlueprintHelperReviewStructurePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewDataAssetPresenterState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
};

