// BlueprintHelper Review native My Blueprint row.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "Widgets/Views/STableRow.h"

class STableViewBase;

class BLUEPRINTHELPER_API SBlueprintHelperReviewMyBlueprintRow
	: public STableRow<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewMyBlueprintRow) {}
		SLATE_ARGUMENT(TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>, Item)
		SLATE_ARGUMENT(FString, AssetPath)
		SLATE_ARGUMENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
		SLATE_ARGUMENT(FBlueprintHelperReviewMyBlueprintNavigateToGraph, OnNavigateToGraph)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable);
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

private:
	FSlateColor GetRowBackgroundColor() const;
	EVisibility GetDiffVisibility() const;
	const FSlateBrush* GetDiffBrush() const;
	EVisibility GetActionVisibility() const;
	EVisibility GetVariableTypeVisibility() const;
	FReply OnAcceptClicked() const;
	FReply OnRejectClicked() const;

	TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item;
	FString AssetPath;
	FBlueprintHelperReviewMyBlueprintNavigateToGraph OnNavigateToGraph;
	mutable FSlateRoundedBoxBrush DiffBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
};
