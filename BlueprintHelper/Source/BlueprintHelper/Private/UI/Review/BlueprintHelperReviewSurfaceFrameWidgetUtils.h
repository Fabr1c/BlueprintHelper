// BlueprintHelper Review surface frame widget utilities.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class SWidget;

class FBlueprintHelperReviewSurfaceFrameWidgetUtils
{
public:
	static TSharedRef<SWidget> BuildDiffFrameWidget(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const TSharedRef<SWidget>& Content,
		bool bShowActions,
		bool bFillBackground,
		const FSlateColor& FrameColor,
		const TFunction<FReply(const FString&)>& OnAcceptChangeId,
		const TFunction<FReply(const FString&)>& OnRejectChangeId,
		bool bSelected);

	static FLinearColor GetReviewFrameBackgroundColor(bool bFillBackground);
	static FLinearColor GetReviewFrameFillColor(
		const FLinearColor& FrameColor,
		bool bFillBackground,
		bool bSelected);

private:
	static FLinearColor GetReviewFrameInnerBg();
};
