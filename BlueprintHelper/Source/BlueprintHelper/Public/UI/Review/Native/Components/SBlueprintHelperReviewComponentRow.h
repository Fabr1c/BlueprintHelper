// BlueprintHelper Review native Components row.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentsPanel.h"
#include "Widgets/Views/STableRow.h"

class STableViewBase;

class BLUEPRINTHELPER_API SBlueprintHelperReviewComponentRow
	: public STableRow<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewComponentRow) {}
		SLATE_ARGUMENT(TSharedPtr<FBlueprintHelperReviewComponentRowItem>, Item)
		SLATE_ARGUMENT(FString, AssetPath)
		SLATE_ARGUMENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable);

private:
	FSlateColor GetRowBackgroundColor() const;
	EVisibility GetDiffVisibility() const;
	const FSlateBrush* GetDiffBrush() const;
	const FSlateBrush* GetComponentIconBrush() const;
	EVisibility GetActionVisibility() const;
	FReply OnAcceptClicked() const;
	FReply OnRejectClicked() const;

	TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item;
	FString AssetPath;
	mutable FSlateRoundedBoxBrush DiffBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
};
