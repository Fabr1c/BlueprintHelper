// BlueprintHelper Review diff frame widget.

#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameWidgetUtils.h"

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
	OnAccept = InArgs._OnAccept;
	OnReject = InArgs._OnReject;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetFrameBrush)
			.Padding(3.0f)
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
			.Padding(5.0f)
			.Visibility(this, &SBlueprintHelperReviewDiffFrame::GetActionsVisibility)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
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
		FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameFillColor(
			GetFrameColor().GetSpecifiedColor(),
			bFillBackground,
			bSelected),
		5.0f);
	return &InnerBrush;
}

EVisibility SBlueprintHelperReviewDiffFrame::GetActionsVisibility() const
{
	return ShowActions.Get(false) && IsHovered()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}
