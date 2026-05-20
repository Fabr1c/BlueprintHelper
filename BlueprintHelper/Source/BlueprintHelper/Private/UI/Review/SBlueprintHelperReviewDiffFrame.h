// BlueprintHelper Review diff frame widget.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SCompoundWidget.h"

class SBlueprintHelperReviewDiffFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewDiffFrame)
		: _FrameColor(FSlateColor(FLinearColor::Transparent))
		, _ShowActions(false)
		, _FillBackground(true)
		, _Selected(false)
		, _FrameOuterPadding(3.0f)
		, _ActionPadding(5.0f)
		, _ActionSpacing(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		, _SurfaceOverlayFillAlpha(0.60f)
		, _SurfaceOverlaySelectedFillAlpha(0.74f)
	{
	}

		SLATE_ATTRIBUTE(FSlateColor, FrameColor)
		SLATE_ATTRIBUTE(bool, ShowActions)
		SLATE_ARGUMENT(bool, FillBackground)
		SLATE_ARGUMENT(bool, Selected)
		SLATE_ARGUMENT(float, FrameOuterPadding)
		SLATE_ARGUMENT(float, ActionPadding)
		SLATE_ARGUMENT(FMargin, ActionSpacing)
		SLATE_ARGUMENT(float, SurfaceOverlayFillAlpha)
		SLATE_ARGUMENT(float, SurfaceOverlaySelectedFillAlpha)
		SLATE_EVENT(FOnClicked, OnAccept)
		SLATE_EVENT(FOnClicked, OnReject)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSlateColor GetFrameColor() const;
	const FSlateBrush* GetFrameBrush() const;
	const FSlateBrush* GetInnerBrush() const;
	EVisibility GetActionsVisibility() const;
	FLinearColor GetFillColor() const;

	TAttribute<FSlateColor> FrameColor;
	TAttribute<bool> ShowActions;
	bool bFillBackground = true;
	bool bSelected = false;
	float FrameOuterPadding = 3.0f;
	float ActionPadding = 5.0f;
	FMargin ActionSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
	float SurfaceOverlayFillAlpha = 0.60f;
	float SurfaceOverlaySelectedFillAlpha = 0.74f;
	FOnClicked OnAccept;
	FOnClicked OnReject;
	mutable FSlateRoundedBoxBrush FrameBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f);
	mutable FSlateRoundedBoxBrush InnerBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 5.0f);
	FSlateRoundedBoxBrush ActionsBrush = FSlateRoundedBoxBrush(
		FLinearColor(0.02f, 0.02f, 0.02f, 0.95f),
		5.0f);
};
