// BlueprintHelper Review diff frame widget.

#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperReviewDiffFrame::Construct(const FArguments& InArgs)
{
	FrameColor = InArgs._FrameColor;
	ShowActions = InArgs._ShowActions;
	bFillBackground = InArgs._FillBackground;
	bSelected = InArgs._Selected;
	FrameOuterPadding = FMath::Max(0.0f, InArgs._FrameOuterPadding);
	ActionPadding = FMath::Max(0.0f, InArgs._ActionPadding);
	ActionSpacing = InArgs._ActionSpacing;
	SurfaceOverlayFillAlpha = FMath::Clamp(InArgs._SurfaceOverlayFillAlpha, 0.0f, 1.0f);
	SurfaceOverlaySelectedFillAlpha = FMath::Clamp(InArgs._SurfaceOverlaySelectedFillAlpha, 0.0f, 1.0f);
	OnAccept = InArgs._OnAccept;
	OnReject = InArgs._OnReject;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetFrameBrush)
			.Padding(FrameOuterPadding)
			[
				SNew(SBorder)
				.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetInnerBrush)
				.Padding(0.0f)
				[
					InArgs._Content.Widget
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SBorder)
			.BorderImage(&ActionsBrush)
			.Padding(ActionPadding)
			.Visibility(this, &SBlueprintHelperReviewDiffFrame::GetActionsVisibility)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ActionSpacing)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Accept")))
					.OnClicked(OnAccept)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Reject")))
					.OnClicked(OnReject)
				]
			]
		]
	];
}

FSlateColor SBlueprintHelperReviewDiffFrame::GetFrameColor() const
{
	return FrameColor.Get();
}

const FSlateBrush* SBlueprintHelperReviewDiffFrame::GetFrameBrush() const
{
	FrameBrush = FSlateRoundedBoxBrush(
		FLinearColor::Transparent,
		7.0f,
		GetFrameColor().GetSpecifiedColor(),
		4.0f);
	return &FrameBrush;
}

const FSlateBrush* SBlueprintHelperReviewDiffFrame::GetInnerBrush() const
{
	InnerBrush = FSlateRoundedBoxBrush(
		GetFillColor(),
		5.0f);
	return &InnerBrush;
}

EVisibility SBlueprintHelperReviewDiffFrame::GetActionsVisibility() const
{
	return ShowActions.Get(false) && IsHovered()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FLinearColor SBlueprintHelperReviewDiffFrame::GetFillColor() const
{
	if (!bFillBackground)
	{
		return FLinearColor::Transparent;
	}

	FLinearColor FillColor = GetFrameColor().GetSpecifiedColor();
	if (FillColor == FLinearColor::Transparent)
	{
		FillColor = FLinearColor(0.06f, 0.06f, 0.06f, SurfaceOverlayFillAlpha);
	}
	FillColor.A = bSelected ? SurfaceOverlaySelectedFillAlpha : SurfaceOverlayFillAlpha;
	return FillColor;
}
