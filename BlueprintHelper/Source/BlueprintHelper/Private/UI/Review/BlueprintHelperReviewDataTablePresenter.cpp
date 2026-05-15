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
#include "Widgets/Input/SButton.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

using FDataTableReviewColumnPtr = TSharedPtr<FDataTableEditorColumnHeaderData>;
using FDataTableReviewRowPtr = TSharedPtr<FDataTableEditorRowListViewData>;

static FString GetDataTableReviewRowName(const FDataTableReviewRowPtr& RowData)
{
	return RowData.IsValid() ? RowData->RowId.ToString() : FString(TEXT("<invalid>"));
}

static FString GetDataTableReviewRowSearchText(const FDataTableReviewRowPtr& RowData)
{
	return FString::Printf(TEXT("datatable_row:%s"), *GetDataTableReviewRowName(RowData));
}

static void BuildDataTableSelectedRowFields(
	const FDataTableReviewRowPtr& RowData,
	const TArray<FDataTableReviewColumnPtr>& Columns,
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& OutRows)
{
	OutRows.Reset();
	if (!RowData.IsValid())
	{
		return;
	}

	const FString RowName = GetDataTableReviewRowName(RowData);
	const FString RowSearchText = GetDataTableReviewRowSearchText(RowData);

	TSharedRef<FBlueprintHelperReviewDataAssetRowItem> SummaryRow =
		MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
	SummaryRow->Label = FString::Printf(TEXT("Row: %s"), *RowName);
	SummaryRow->Value = FString::Printf(TEXT("Fields: %d"), Columns.Num());
	SummaryRow->SearchText = RowSearchText;
	SummaryRow->bIsSection = true;
	OutRows.Add(SummaryRow);

	for (int32 ColumnIndex = 0; ColumnIndex < Columns.Num(); ++ColumnIndex)
	{
		const FDataTableReviewColumnPtr& Column = Columns[ColumnIndex];
		if (!Column.IsValid())
		{
			continue;
		}

		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> FieldRow =
			MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		const FString FieldName = Column->DisplayName.ToString();
		const FString FieldValue = RowData->CellData.IsValidIndex(ColumnIndex)
			? RowData->CellData[ColumnIndex].ToString()
			: FString();
		FieldRow->Label = FieldName;
		FieldRow->Value = FieldValue;
		FieldRow->Depth = 1;
		FieldRow->SearchText = FString::Printf(
			TEXT("%s datatable_cell:%s.%s data_table_cell:%s.%s %s %s"),
			*RowSearchText,
			*RowName,
			*FieldName,
			*RowName,
			*FieldName,
			*FieldName,
			*FieldValue);
		OutRows.Add(FieldRow);
	}
}

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
		const FString RowName = GetDataTableReviewRowName(RowData);
		const FString SearchText = GetDataTableReviewRowSearchText(RowData);
		const bool bActionsColumn = ColumnName == FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableActionsColumnId();
		TSharedRef<SWidget> Cell = bActionsColumn ? BuildHoverActions(SearchText) : BuildCellWidget(ColumnName);
		TSharedRef<SWidget> RowCell = SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(EBlueprintHelperReviewSurface::DataTable)
			.TargetKey(SearchText)
			.OnGeometryInvalidated(OnGeometryInvalidated)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
				.BorderBackgroundColor_Lambda([AssetPath = AssetPath, SearchText]()
				{
					return FBlueprintHelperReviewPresenterWidgetUtils::GetRowBackgroundOrDefault(
						AssetPath,
						EBlueprintHelperReviewSurface::DataTable,
						SearchText,
						FLinearColor::Transparent);
				})
				.Padding(FMargin(4.0f, 3.0f))
				[
					Cell
				]
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
	TSharedRef<SWidget> BuildHoverActions(const FString& SearchText) const
	{
		return SNew(SHorizontalBox)
			.Visibility_Lambda([this, SearchText]()
			{
				const bool bHasDiff = FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
					AssetPath,
					EBlueprintHelperReviewSurface::DataTable,
					SearchText).GetSpecifiedColor().A > 0.0f;
				const bool bSelected = FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
					AssetPath,
					EBlueprintHelperReviewSurface::DataTable,
					SearchText) == EVisibility::Visible;
				return bHasDiff && (IsHovered() || bSelected)
					? EVisibility::Visible
					: EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Accept")))
				.OnClicked_Lambda([AssetPath = AssetPath, SearchText]()
				{
					return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
						AssetPath,
						EBlueprintHelperReviewSurface::DataTable,
						SearchText);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Reject")))
				.OnClicked_Lambda([AssetPath = AssetPath, SearchText]()
				{
					return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
						AssetPath,
						EBlueprintHelperReviewSurface::DataTable,
						SearchText);
				})
			];
	}

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
	State.SelectedRowFields.Reset();
	State.SelectedRow.Reset();
	State.ListView.Reset();
	State.SelectedRowFieldListView.Reset();

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
	if (State.Rows.Num() > 0)
	{
		State.SelectedRow = State.Rows[0];
		BuildDataTableSelectedRowFields(State.SelectedRow, State.Columns, State.SelectedRowFields);
	}

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
	FBlueprintHelperReviewDataTablePresenterState* StatePtr = &State;
	TSharedRef<SListView<FDataTableReviewRowPtr>> ListView =
		SAssignNew(State.ListView, SListView<FDataTableReviewRowPtr>)
		.ListItemsSource(&State.Rows)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow(HeaderRow)
		.OnSelectionChanged_Lambda([StatePtr, ColumnSource, OnGeometryInvalidated](
			FDataTableReviewRowPtr SelectedRow,
			ESelectInfo::Type)
		{
			if (!StatePtr)
			{
				return;
			}
			StatePtr->SelectedRow = SelectedRow;
			BuildDataTableSelectedRowFields(SelectedRow, *ColumnSource, StatePtr->SelectedRowFields);
			if (StatePtr->SelectedRowFieldListView.IsValid())
			{
				StatePtr->SelectedRowFieldListView->RequestListRefresh();
			}
			OnGeometryInvalidated.ExecuteIfBound(EBlueprintHelperReviewSurface::DataTable);
		})
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
	if (State.SelectedRow.IsValid())
	{
		ListView->SetSelection(State.SelectedRow);
	}

	TSharedRef<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> SelectedRowFieldListView =
		SAssignNew(State.SelectedRowFieldListView, SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>)
		.ListItemsSource(&State.SelectedRowFields)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return FBlueprintHelperReviewPresenterWidgetUtils::GenerateDataAssetRow(
				Item,
				OwnerTable,
				AssetPath,
				OnGeometryInvalidated,
				EBlueprintHelperReviewSurface::DataTable,
				FMargin(4.0f, 3.0f));
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
			.FillHeight(0.42f)
			[
				ListView
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Selected Row Details")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f)))
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.58f)
			[
				SelectedRowFieldListView
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
