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
	{
	}

		SLATE_ATTRIBUTE(FSlateColor, FrameColor)
		SLATE_ATTRIBUTE(bool, ShowActions)
		SLATE_ARGUMENT(bool, FillBackground)
		SLATE_ARGUMENT(bool, Selected)
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

	TAttribute<FSlateColor> FrameColor;
	TAttribute<bool> ShowActions;
	bool bFillBackground = true;
	bool bSelected = false;
	FOnClicked OnAccept;
	FOnClicked OnReject;
	mutable FSlateRoundedBoxBrush FrameBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f);
	mutable FSlateRoundedBoxBrush InnerBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 5.0f);
	FSlateRoundedBoxBrush ActionsBrush = FSlateRoundedBoxBrush(
		FLinearColor(0.02f, 0.02f, 0.02f, 0.95f),
		5.0f);
};
