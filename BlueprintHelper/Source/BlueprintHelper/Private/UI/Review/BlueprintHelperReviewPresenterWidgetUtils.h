// BlueprintHelper Review presenter widget utility helpers.

#pragma once

#include <initializer_list>

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/Views/STableViewBase.h"

class IDetailTreeNode;
class UWidget;
class UWidgetTree;
struct FDataTableEditorColumnHeaderData;
struct FDataTableEditorRowListViewData;

class FBlueprintHelperReviewPresenterWidgetUtils
{
public:
	static bool TargetKindEqualsAny(
		const FString& TargetKind,
		std::initializer_list<const TCHAR*> ExpectedKinds);

	static bool ChangeHasTargetKind(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds);

	static bool IsSurfaceRoutable(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static bool ShouldShowIndependentSurfaceChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds);

	static TSharedRef<SWidget> BuildLine(const FString& Text, const FLinearColor& Color);

	static TSharedRef<SWidget> BuildRegisteredLine(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& Text,
		const FLinearColor& Color,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated);

	static TSharedRef<SWidget> BuildSummaryPanel(
		const FString& Title,
		const TArray<FString>& Lines,
		const FString& AssetPath = FString(),
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());

	static FString GetAssetShortName(const FString& AssetPath);
	static FString ExtractReadableTail(FString Text);

	static FSlateColor GetRowBackgroundOrDefault(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		const FLinearColor& DefaultColor);

	static TSharedRef<SWidget> BuildRowActions(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		TWeakPtr<SWidget> HoverSource = TWeakPtr<SWidget>());

	static TSharedRef<SWidget> BuildRowHighlightShell(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		TSharedRef<SWidget> Content,
		const FLinearColor& DefaultBackground = FLinearColor::Transparent,
		const FMargin& Padding = FMargin(4.0f, 2.0f));

	static void RegisterRowSearchAliases(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& PrimaryKey,
		const TSharedRef<SWidget>& RowWidget,
		const TCHAR* DebugMode);

	static TSharedRef<SWidget> BuildAssetSummaryRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& Label,
		const FString& SearchText,
		const FLinearColor& DefaultBackground = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));

	static FString GetWidgetTreeClassText(const UWidget* Widget);

	static TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> MakeWidgetTreeRowItem(
		FName WidgetName,
		const FString& WidgetClass,
		int32 Depth);

	static TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> BuildWidgetTreeRowItem(
		UWidget* Widget,
		int32 Depth,
		TSet<const UWidget*>& ReachableWidgets);

	static void CollectUnparentedWidgetTreeItems(
		UWidgetTree* WidgetTree,
		const TSet<const UWidget*>& ReachableWidgets,
		TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>& OutItems);

	static bool IsWidgetTreeGroupRow(const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item);

	static void RegisterWidgetTreeRowAliases(
		const FString& AssetPath,
		const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item,
		const TSharedRef<SWidget>& RowWidget);

	static TSharedRef<ITableRow> GenerateWidgetTreeRow(
		TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable,
		const FString& AssetPath,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated);

	static void ExpandWidgetTreeRows(
		const TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>>& TreeView,
		const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item);

	static const FName& GetDataTableRowNameColumnId();
	static const FName& GetDataTableActionsColumnId();

	static FString GetDetailNodeSearchText(const TSharedPtr<IDetailTreeNode>& DetailNode);

	static void FlattenDetailTreeNodes(
		const TArray<TSharedRef<IDetailTreeNode>>& Nodes,
		int32 Depth,
		TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& OutRows);

	static TSharedRef<SWidget> BuildDataAssetRowContent(
		const TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>& Item);

	static TSharedRef<ITableRow> GenerateDataAssetRow(
		TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable,
		const FString& AssetPath,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated,
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::DataAsset,
		const FMargin& HighlightPadding = FMargin(4.0f, 2.0f));
};

