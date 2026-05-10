// BlueprintHelper Review asset presenter shared data types.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class IDetailTreeNode;
class IPropertyRowGenerator;
class SWidget;
struct FDataTableEditorColumnHeaderData;
struct FDataTableEditorRowListViewData;
template <typename ItemType>
class STreeView;
template <typename ItemType>
class SListView;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewWidgetTreeRowItem
{
	FName WidgetName;
	FString WidgetClass;
	int32 Depth = 0;
	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> Children;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewWidgetTreePresenterState
{
	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>> TreeView;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataTablePresenterState
{
	TArray<TSharedPtr<FDataTableEditorColumnHeaderData>> Columns;
	TArray<TSharedPtr<FDataTableEditorRowListViewData>> Rows;
	TSharedPtr<SListView<TSharedPtr<FDataTableEditorRowListViewData>>> ListView;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetRowItem
{
	FString Label;
	FString Value;
	FString SearchText;
	int32 Depth = 0;
	bool bIsSection = false;
	TSharedPtr<IDetailTreeNode> DetailNode;
	TWeakPtr<SWidget> RowWidget;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenterState
{
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>> Rows;
	TSharedPtr<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView;
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
};
