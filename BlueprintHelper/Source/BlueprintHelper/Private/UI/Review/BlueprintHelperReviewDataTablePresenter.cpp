// BlueprintHelper Review DataTable presenter.

#include "UI/Review/BlueprintHelperReviewDataTablePresenter.h"

#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

using FDataTableReviewColumnPtr = TSharedPtr<FDataTableEditorColumnHeaderData>;
using FDataTableReviewRowPtr = TSharedPtr<FDataTableEditorRowListViewData>;

class SBlueprintHelperReviewDataTableRow : public SMultiColumnTableRow<FDataTableReviewRowPtr>
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewDataTableRow) {}
		SLATE_ARGUMENT(FDataTableReviewRowPtr, RowData)
		SLATE_ARGUMENT(TArray<FDataTableReviewColumnPtr>*, Columns)
		SLATE_ARGUMENT(FString, AssetPath)
		SLATE_EVENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		RowData = InArgs._RowData;
		Columns = InArgs._Columns;
		AssetPath = InArgs._AssetPath;
		OnGeometryInvalidated = InArgs._OnGeometryInvalidated;

		SMultiColumnTableRow<FDataTableReviewRowPtr>::Construct(
			SMultiColumnTableRow<FDataTableReviewRowPtr>::FArguments()
				.Style(FAppStyle::Get(), TEXT("DataTableEditor.CellListViewRow")),
			InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		const FString RowName = RowData.IsValid() ? RowData->RowId.ToString() : FString(TEXT("<invalid>"));
		const FString SearchText = FString::Printf(TEXT("datatable_row:%s"), *RowName);
		TSharedRef<SWidget> Cell = BuildCellWidget(ColumnName);
		TSharedRef<SWidget> RowCell = SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(EBlueprintHelperReviewSurface::DataTable)
			.TargetKey(SearchText)
			.OnGeometryInvalidated(OnGeometryInvalidated)
			[
				FBlueprintHelperReviewPresenterWidgetUtils::BuildRowHighlightShell(
					AssetPath,
					EBlueprintHelperReviewSurface::DataTable,
					SearchText,
					Cell)
			];

		FBlueprintHelperReviewPresenterWidgetUtils::RegisterRowSearchAliases(
			AssetPath,
			EBlueprintHelperReviewSurface::DataTable,
			SearchText,
			RowCell,
			TEXT("native_datatable_row"));
		FBlueprintHelperReviewPresenterWidgetUtils::RegisterRowSearchAliases(
			AssetPath,
			EBlueprintHelperReviewSurface::DataTable,
			RowName,
			RowCell,
			TEXT("native_datatable_row"));
		return RowCell;
	}

private:
	TSharedRef<SWidget> BuildCellWidget(const FName& ColumnName) const
	{
		if (!RowData.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		if (ColumnName == FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableActionsColumnId())
		{
			return SNew(SSpacer)
				.Size(FVector2D(1.0f, 1.0f));
		}

		if (ColumnName == FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableRowNameColumnId())
		{
			return SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), TEXT("DataTableEditor.CellText"))
				.Text(RowData->DisplayName);
		}

		const int32 ColumnIndex = Columns
			? Columns->IndexOfByPredicate([ColumnName](const FDataTableReviewColumnPtr& Column)
			{
				return Column.IsValid() && Column->ColumnId == ColumnName;
			})
			: INDEX_NONE;

		if (ColumnIndex != INDEX_NONE && RowData->CellData.IsValidIndex(ColumnIndex))
		{
			return SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), TEXT("DataTableEditor.CellText"))
				.Text(RowData->CellData[ColumnIndex])
				.ToolTipText(RowData->CellData[ColumnIndex]);
		}

		return SNullWidget::NullWidget;
	}

	FDataTableReviewRowPtr RowData;
	TArray<FDataTableReviewColumnPtr>* Columns = nullptr;
	FString AssetPath;
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
};

bool FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataTable,
		{TEXT("datatable_row"), TEXT("data_table"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewDataTablePresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.Columns.Reset();
	State.Rows.Reset();
	State.ListView.Reset();

	UDataTable* DataTable = Context.DataTable.Get();
	if (!DataTable)
	{
		DataTable = Cast<UDataTable>(Context.AssetObject.Get());
	}

	if (!DataTable)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
		Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));
		Lines.Add(TEXT("DataTable: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("DataTable Summary"),
			Lines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	FDataTableEditorUtils::CacheDataTableForEditing(DataTable, State.Columns, State.Rows);

	TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableRowNameColumnId())
		.DefaultLabel(FText::FromString(TEXT("Row")))
		.ManualWidth(160.0f);
	for (const FDataTableReviewColumnPtr& Column : State.Columns)
	{
		if (!Column.IsValid())
		{
			continue;
		}
		HeaderRow->AddColumn(
			SHeaderRow::Column(Column->ColumnId)
			.DefaultLabel(Column->DisplayName)
			.ManualWidth(FMath::Max(96.0f, Column->DesiredColumnWidth)));
	}
	HeaderRow->AddColumn(
		SHeaderRow::Column(FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableActionsColumnId())
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(148.0f));

	const FString AssetPath = Context.AssetPath;
	TArray<FDataTableReviewColumnPtr>* ColumnSource = &State.Columns;
	TSharedRef<SListView<FDataTableReviewRowPtr>> ListView =
		SAssignNew(State.ListView, SListView<FDataTableReviewRowPtr>)
		.ListItemsSource(&State.Rows)
		.SelectionMode(ESelectionMode::None)
		.HeaderRow(HeaderRow)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated, ColumnSource](
			FDataTableReviewRowPtr RowData,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return SNew(SBlueprintHelperReviewDataTableRow, OwnerTable)
				.RowData(RowData)
				.Columns(ColumnSource)
				.AssetPath(AssetPath)
				.OnGeometryInvalidated(OnGeometryInvalidated);
		});

	const FString AssetName = FBlueprintHelperReviewPresenterWidgetUtils::GetAssetShortName(Context.AssetPath);
	const FString SummarySearchText = FString::Printf(
		TEXT("asset_factory:data_table data_table %s %s"),
		*AssetName,
		*Context.AssetPath);
	return SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				FBlueprintHelperReviewPresenterWidgetUtils::BuildAssetSummaryRow(
					Context.AssetPath,
					EBlueprintHelperReviewSurface::DataTable,
					FString::Printf(
						TEXT("DataTable: %s  Rows: %d  Columns: %d"),
						*AssetName,
						State.Rows.Num(),
						State.Columns.Num()),
					SummarySearchText)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				ListView
			]
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataTable,
		&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange);
}
