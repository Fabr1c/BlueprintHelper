// BlueprintHelper Review surface frame widget utilities.

#include "UI/Review/BlueprintHelperReviewSurfaceFrameWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"

static float GBlueprintHelperReviewFrameOuterPadding = 3.0f;
static float GBlueprintHelperReviewFrameActionPadding = 5.0f;
static FMargin GBlueprintHelperReviewFrameActionSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
static float GBlueprintHelperReviewFrameFillAlpha = 0.60f;
static float GBlueprintHelperReviewFrameSelectedFillAlpha = 0.74f;

void BlueprintHelperReviewSetSurfaceFrameWidgetStyle(
	const float FrameOuterPadding,
	const float ActionPadding,
	const FMargin& ActionSpacing,
	const float FillAlpha,
	const float SelectedFillAlpha)
{
	GBlueprintHelperReviewFrameOuterPadding = FMath::Max(0.0f, FrameOuterPadding);
	GBlueprintHelperReviewFrameActionPadding = FMath::Max(0.0f, ActionPadding);
	GBlueprintHelperReviewFrameActionSpacing = ActionSpacing;
	GBlueprintHelperReviewFrameFillAlpha = FMath::Clamp(FillAlpha, 0.0f, 1.0f);
	GBlueprintHelperReviewFrameSelectedFillAlpha = FMath::Clamp(SelectedFillAlpha, 0.0f, 1.0f);
}

void BlueprintHelperReviewSetSurfaceFrameOverlayAlpha(const float FillAlpha, const float SelectedFillAlpha)
{
	BlueprintHelperReviewSetSurfaceFrameWidgetStyle(
		GBlueprintHelperReviewFrameOuterPadding,
		GBlueprintHelperReviewFrameActionPadding,
		GBlueprintHelperReviewFrameActionSpacing,
		FillAlpha,
		SelectedFillAlpha);
}

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
		.FrameOuterPadding(GBlueprintHelperReviewFrameOuterPadding)
		.ActionPadding(GBlueprintHelperReviewFrameActionPadding)
		.ActionSpacing(GBlueprintHelperReviewFrameActionSpacing)
		.SurfaceOverlayFillAlpha(GBlueprintHelperReviewFrameFillAlpha)
		.SurfaceOverlaySelectedFillAlpha(GBlueprintHelperReviewFrameSelectedFillAlpha)
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
	if (!bFillBackground)
	{
		return FLinearColor::Transparent;
	}

	FLinearColor FillColor = FrameColor;
	if (FillColor == FLinearColor::Transparent)
	{
		FillColor = GetReviewFrameInnerBg();
	}
	FillColor.A = bSelected ? GBlueprintHelperReviewFrameSelectedFillAlpha : GBlueprintHelperReviewFrameFillAlpha;
	return FillColor;
}

FLinearColor FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameInnerBg()
{
	return FLinearColor(0.06f, 0.06f, 0.06f, GBlueprintHelperReviewFrameFillAlpha);
}
