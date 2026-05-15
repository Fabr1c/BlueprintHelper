// BlueprintHelper Review native Components panel.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STreeView.h"

class UBlueprint;
class USCS_Node;

struct FBlueprintHelperReviewComponentRowItem
{
	FString ComponentName;
	FString DisplayName;
	FString ComponentClass;
	FText ToolTipText;
	TWeakObjectPtr<UClass> ComponentClassObject;
	int32 Depth = 0;
	TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>> Children;
	TWeakPtr<SWidget> RowWidget;
};

class BLUEPRINTHELPER_API SBlueprintHelperReviewComponentsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewComponentsPanel) {}
		SLATE_ARGUMENT(UBlueprint*, Blueprint)
		SLATE_ARGUMENT(FString, AssetPath)
		SLATE_ARGUMENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<FBlueprintHelperReviewComponentRowItem> FindRowByCandidates(const TArray<FString>& Candidates) const;
	TSharedPtr<SWidget> GetRowWidgetForItem(const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item) const;
	void RequestScrollIntoView(const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item) const;

private:
	void RebuildItems(UBlueprint* Blueprint);
	TSharedPtr<FBlueprintHelperReviewComponentRowItem> BuildItemFromSCSNode(USCS_Node* Node, int32 Depth) const;
	TSharedPtr<FBlueprintHelperReviewComponentRowItem> FindRowByCandidatesRecursive(
		const TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>& Items,
		const TArray<FString>& Candidates) const;
	static bool SearchTextMatchesAnyCandidate(const FString& SearchText, const TArray<FString>& Candidates);
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);

	TSharedRef<ITableRow> OnGenerateRow(
		TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(
		TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item,
		TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>& OutChildren) const;

	TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>> RootItems;
	FString AssetPath;
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>> TreeView;
};
