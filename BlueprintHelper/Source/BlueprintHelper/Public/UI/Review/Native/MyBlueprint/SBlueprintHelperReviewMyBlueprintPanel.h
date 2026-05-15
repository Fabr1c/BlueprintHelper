// BlueprintHelper Review native My Blueprint panel.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class BLUEPRINTHELPER_API SBlueprintHelperReviewMyBlueprintPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewMyBlueprintPanel) {}
		SLATE_ARGUMENT(TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>*, RootItemsSource)
		SLATE_ARGUMENT(FString, AssetPath)
		SLATE_ARGUMENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>> GetTreeView() const
	{
		return TreeView;
	}

private:
	TSharedRef<ITableRow> OnGenerateRow(
		TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(
		TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item,
		TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& OutChildren) const;
	void ExpandItemRecursive(TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item) const;

	TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>* RootItemsSource = nullptr;
	FString AssetPath;
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>> TreeView;
};
