// BlueprintHelper Review surface frame widget utilities.

#include "UI/Review/BlueprintHelperReviewSurfaceFrameWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameWidgetUtils::BuildDiffFrameWidget(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions,
	bool bFillBackground,
	const FSlateColor& FrameColor,
	const TFunction<FReply(const FBlueprintHelperReviewActionIntent&)>& OnReviewActionIntent,
	bool bSelected)
{
	return SNew(SBlueprintHelperReviewDiffFrame)
		.FrameColor(FrameColor)
		.ShowActions(bShowActions && Item.IsValid())
		.FillBackground(bFillBackground)
		.Selected(bSelected)
		.OnAccept(FOnClicked::CreateLambda([Item, OnReviewActionIntent]()
		{
			return OnReviewActionIntent && Item.IsValid()
				? OnReviewActionIntent(FBlueprintHelperReviewActionIntent::Accept(
					FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
						*Item,
						EBlueprintHelperReviewSurface::Unknown,
						Item->LocationKey),
					TEXT("diff_frame")))
				: FReply::Handled();
		}))
		.OnReject(FOnClicked::CreateLambda([Item, OnReviewActionIntent]()
		{
			return OnReviewActionIntent && Item.IsValid()
				? OnReviewActionIntent(FBlueprintHelperReviewActionIntent::Reject(
					FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
						*Item,
						EBlueprintHelperReviewSurface::Unknown,
						Item->LocationKey),
					TEXT("diff_frame")))
				: FReply::Handled();
		}))
		[
			Content
		];
}

FLinearColor FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameBackgroundColor(bool bFillBackground)
{
	return bFillBackground ? GetReviewFrameInnerBg() : FLinearColor::Transparent;
}

FLinearColor FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameFillColor(
	const FLinearColor& FrameColor,
	bool bFillBackground,
	bool bSelected)
{
	constexpr float ReviewFrameBackgroundOpacity = 0.60f;
	constexpr float ReviewFrameSelectedBackgroundOpacity = 0.74f;

	if (!bFillBackground)
	{
		return FLinearColor::Transparent;
	}

	FLinearColor FillColor = FrameColor;
	if (FillColor == FLinearColor::Transparent)
	{
		FillColor = GetReviewFrameInnerBg();
	}
	FillColor.A = bSelected ? ReviewFrameSelectedBackgroundOpacity : ReviewFrameBackgroundOpacity;
	return FillColor;
}

FLinearColor FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameInnerBg()
{
	constexpr float ReviewFrameBackgroundOpacity = 0.60f;
	return FLinearColor(0.06f, 0.06f, 0.06f, ReviewFrameBackgroundOpacity);
}
