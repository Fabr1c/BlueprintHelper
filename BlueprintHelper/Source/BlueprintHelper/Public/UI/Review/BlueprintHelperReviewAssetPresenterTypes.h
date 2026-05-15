// BlueprintHelper Review asset presenter shared data types.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class IDetailTreeNode;
class IPropertyRowGenerator;
class SWidget;
struct FDataTableEditorColumnHeaderData;
struct FDataTableEditorRowListViewData;
struct FBlueprintHelperReviewDataAssetRowItem;
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
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>> SelectedRowFields;
	TSharedPtr<FDataTableEditorRowListViewData> SelectedRow;
	TSharedPtr<SListView<TSharedPtr<FDataTableEditorRowListViewData>>> ListView;
	TSharedPtr<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> SelectedRowFieldListView;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetRowItem
{
	FString Label;
	FString Value;
	FString SearchText;
	FEdGraphPinType PinType;
	int32 Depth = 0;
	bool bIsSection = false;
	bool bHasPinType = false;
	TSharedPtr<IDetailTreeNode> DetailNode;
	TWeakPtr<SWidget> RowWidget;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenterState
{
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>> Rows;
	TSharedPtr<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView;
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
};
