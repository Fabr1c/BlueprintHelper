// BlueprintHelper Review asset-specific presenters.

#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"

#include <initializer_list>

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

namespace BlueprintHelperReviewAssetPresentersPrivate
{
	static bool TargetKindEqualsAny(
		const FString& TargetKind,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		FString Normalized = TargetKind;
		Normalized.ToLowerInline();
		for (const TCHAR* ExpectedKind : ExpectedKinds)
		{
			if (Normalized == ExpectedKind)
			{
				return true;
			}
		}
		return false;
	}

	static bool ChangeHasTargetKind(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.Surface != Surface)
			{
				continue;
			}
			if (TargetKindEqualsAny(Target.TargetKind, ExpectedKinds))
			{
				return true;
			}
		}

		FString Location = Change.LocationKey;
		Location.ToLowerInline();
		for (const TCHAR* ExpectedKind : ExpectedKinds)
		{
			if (Location.Contains(ExpectedKind))
			{
				return true;
			}
		}
		return false;
	}

	static TSharedRef<SWidget> BuildLine(const FString& Text, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(Color))
			.AutoWrapText(true)
			.Text(FText::FromString(Text));
	}

	static TSharedRef<SWidget> BuildRegisteredLine(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& Text,
		const FLinearColor& Color,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
	{
		TSharedPtr<SBox> RowBox;
		TSharedRef<SWidget> RowContent = SAssignNew(RowBox, SBox)
		[
			BuildLine(Text, Color)
		];
		TSharedRef<SWidget> RowWidget =
			SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(Surface)
			.TargetKey(Text)
			.OnGeometryInvalidated(OnGeometryInvalidated)
			[
				RowContent
			];

		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			Surface,
			Text,
			RowWidget);
		return RowWidget;
	}

	static TSharedRef<SWidget> BuildSummaryPanel(
		const FString& Title,
		const TArray<FString>& Lines,
		const FString& AssetPath = FString(),
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated())
	{
		TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
		Scroll->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildLine(Title, FLinearColor(0.84f, 0.84f, 0.84f, 1.0f))
		];
		for (const FString& Line : Lines)
		{
			Scroll->AddSlot()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				BuildRegisteredLine(
					AssetPath,
					Surface,
					Line,
					FLinearColor(0.62f, 0.62f, 0.62f, 1.0f),
					OnGeometryInvalidated)
			];
		}

		return SNew(SBorder)
			.Padding(8.0f)
			[
				Scroll
			];
	}

	using FWidgetTreeRowItemPtr = TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>;

	static FString GetWidgetTreeClassText(const UWidget* Widget)
	{
		if (!Widget || !Widget->GetClass())
		{
			return TEXT("Unknown");
		}
		return Widget->GetClass()->GetName();
	}

	static FWidgetTreeRowItemPtr MakeWidgetTreeRowItem(
		const FName WidgetName,
		const FString& WidgetClass,
		const int32 Depth)
	{
		FWidgetTreeRowItemPtr Item = MakeShared<FBlueprintHelperReviewWidgetTreeRowItem>();
		Item->WidgetName = WidgetName;
		Item->WidgetClass = WidgetClass;
		Item->Depth = Depth;
		return Item;
	}

	static FWidgetTreeRowItemPtr BuildWidgetTreeRowItem(
		UWidget* Widget,
		const int32 Depth,
		TSet<const UWidget*>& ReachableWidgets)
	{
		if (!Widget || ReachableWidgets.Contains(Widget))
		{
			return nullptr;
		}

		ReachableWidgets.Add(Widget);
		FWidgetTreeRowItemPtr Item = MakeWidgetTreeRowItem(
			Widget->GetFName(),
			GetWidgetTreeClassText(Widget),
			Depth);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			const int32 ChildCount = Panel->GetChildrenCount();
			for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
			{
				if (FWidgetTreeRowItemPtr ChildItem = BuildWidgetTreeRowItem(
					Panel->GetChildAt(ChildIndex),
					Depth + 1,
					ReachableWidgets))
				{
					Item->Children.Add(ChildItem);
				}
			}
		}

		return Item;
	}

	static void CollectUnparentedWidgetTreeItems(
		UWidgetTree* WidgetTree,
		const TSet<const UWidget*>& ReachableWidgets,
		TArray<FWidgetTreeRowItemPtr>& OutItems)
	{
		if (!WidgetTree)
		{
			return;
		}

		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		AllWidgets.Sort([](const UWidget& Left, const UWidget& Right)
		{
			return Left.GetName() < Right.GetName();
		});

		for (UWidget* Widget : AllWidgets)
		{
			if (!Widget || ReachableWidgets.Contains(Widget))
			{
				continue;
			}
			OutItems.Add(MakeWidgetTreeRowItem(
				Widget->GetFName(),
				GetWidgetTreeClassText(Widget),
				1));
		}
	}

	static bool IsWidgetTreeGroupRow(const FWidgetTreeRowItemPtr& Item)
	{
		return Item.IsValid()
			&& Item->WidgetName == FName(TEXT("Unparented Widgets"))
			&& Item->WidgetClass.IsEmpty();
	}

	static void RegisterWidgetTreeRowAliases(
		const FString& AssetPath,
		const FWidgetTreeRowItemPtr& Item,
		const TSharedRef<SWidget>& RowWidget)
	{
		if (AssetPath.IsEmpty() || !Item.IsValid() || IsWidgetTreeGroupRow(Item))
		{
			return;
		}

		const FString WidgetName = Item->WidgetName.ToString();
		if (WidgetName.IsEmpty())
		{
			return;
		}

		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			FString::Printf(TEXT("umg_widget:%s"), *WidgetName),
			RowWidget,
			TEXT("owned_tree_row"));
		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			FString::Printf(TEXT("umg_widget_property:%s"), *WidgetName),
			RowWidget,
			TEXT("owned_tree_row"));
		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			WidgetName,
			RowWidget,
			TEXT("owned_tree_row"));
	}

	static TSharedRef<ITableRow> GenerateWidgetTreeRow(
		const FWidgetTreeRowItemPtr Item,
		const TSharedRef<STableViewBase>& OwnerTable,
		const FString AssetPath,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
	{
		const FString WidgetName = Item.IsValid()
			? Item->WidgetName.ToString()
			: FString(TEXT("<invalid>"));
		const FString WidgetClass = Item.IsValid()
			? Item->WidgetClass
			: FString();
		const float DepthPadding = Item.IsValid()
			? static_cast<float>(FMath::Max(0, Item->Depth) * 12)
			: 0.0f;
		const FString ProbeTargetKey = WidgetName.IsEmpty() ? FString(TEXT("widget_tree_row")) : WidgetName;
		TSharedRef<SWidget> RowContent =
			SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(EBlueprintHelperReviewSurface::UMGWidgetTree)
			.TargetKey(ProbeTargetKey)
			.OnGeometryInvalidated(OnGeometryInvalidated)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
				.BorderBackgroundColor_Lambda([AssetPath, ProbeTargetKey]()
				{
					return FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
						AssetPath,
						EBlueprintHelperReviewSurface::UMGWidgetTree,
						ProbeTargetKey);
				})
				.Padding(FMargin(4.0f, 2.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(WidgetName))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.84f, 0.84f, 1.0f)))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(WidgetClass.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Text(FText::FromString(WidgetClass))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.52f, 0.52f, 0.52f, 1.0f)))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([AssetPath, ProbeTargetKey]()
						{
							return FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
								AssetPath,
								EBlueprintHelperReviewSurface::UMGWidgetTree,
								ProbeTargetKey);
						})
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Accept")))
							.OnClicked_Lambda([AssetPath, ProbeTargetKey]()
							{
								return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
									AssetPath,
									EBlueprintHelperReviewSurface::UMGWidgetTree,
									ProbeTargetKey);
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Reject")))
							.OnClicked_Lambda([AssetPath, ProbeTargetKey]()
							{
								return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
									AssetPath,
									EBlueprintHelperReviewSurface::UMGWidgetTree,
									ProbeTargetKey);
							})
						]
					]
				]
			];

		TSharedRef<STableRow<FWidgetTreeRowItemPtr>> RowWidget =
			SNew(STableRow<FWidgetTreeRowItemPtr>, OwnerTable)
			.Padding(FMargin(DepthPadding, 2.0f, 4.0f, 2.0f))
			[
				RowContent
			];

		RegisterWidgetTreeRowAliases(AssetPath, Item, RowContent);
		return RowWidget;
	}

	static void ExpandWidgetTreeRows(
		const TSharedPtr<STreeView<FWidgetTreeRowItemPtr>>& TreeView,
		const FWidgetTreeRowItemPtr& Item)
	{
		if (!TreeView.IsValid() || !Item.IsValid())
		{
			return;
		}

		TreeView->SetItemExpansion(Item, true);
		for (const FWidgetTreeRowItemPtr& Child : Item->Children)
		{
			ExpandWidgetTreeRows(TreeView, Child);
		}
	}

	static bool IsSurfaceRoutable(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface)
	{
		return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(Change, Surface);
	}

	static bool ShouldShowIndependentSurfaceChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		if (!IsSurfaceRoutable(Change, Surface))
		{
			return false;
		}

		if (BlueprintHelperReviewHasExplicitTargets(Change))
		{
			return ChangeHasTargetKind(Change, Surface, ExpectedKinds);
		}
		return true;
	}

	static FString GetAssetShortName(const FString& AssetPath)
	{
		FString Trimmed = AssetPath;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}

		int32 SeparatorIndex = INDEX_NONE;
		if (Trimmed.FindLastChar(TEXT('.'), SeparatorIndex)
			|| Trimmed.FindLastChar(TEXT('/'), SeparatorIndex))
		{
			Trimmed = Trimmed.Mid(SeparatorIndex + 1);
		}
		Trimmed.TrimStartAndEndInline();
		return Trimmed;
	}

	static FString ExtractReadableTail(FString Text)
	{
		Text.TrimStartAndEndInline();
		int32 SeparatorIndex = INDEX_NONE;
		if (Text.FindLastChar(TEXT(':'), SeparatorIndex)
			|| Text.FindLastChar(TEXT('/'), SeparatorIndex)
			|| Text.FindLastChar(TEXT('.'), SeparatorIndex))
		{
			Text = Text.Mid(SeparatorIndex + 1);
		}
		Text.TrimStartAndEndInline();
		return Text;
	}

	static FSlateColor GetRowBackgroundOrDefault(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		const FLinearColor& DefaultColor)
	{
		const FSlateColor HighlightColor =
			FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(AssetPath, Surface, SearchText);
		if (HighlightColor.GetSpecifiedColor().A > 0.0f)
		{
			return HighlightColor;
		}
		return FSlateColor(DefaultColor);
	}

	static TSharedRef<SWidget> BuildRowActions(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText)
	{
		return SNew(SHorizontalBox)
			.Visibility_Lambda([AssetPath, Surface, SearchText]()
			{
				return FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
					AssetPath,
					Surface,
					SearchText);
			})
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Accept")))
				.OnClicked_Lambda([AssetPath, Surface, SearchText]()
				{
					return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
						AssetPath,
						Surface,
						SearchText);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Reject")))
				.OnClicked_Lambda([AssetPath, Surface, SearchText]()
				{
					return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
						AssetPath,
						Surface,
						SearchText);
				})
			];
	}

	static TSharedRef<SWidget> BuildRowHighlightShell(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		TSharedRef<SWidget> Content,
		const FLinearColor& DefaultBackground = FLinearColor::Transparent)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
			.BorderBackgroundColor_Lambda([AssetPath, Surface, SearchText, DefaultBackground]()
			{
				return GetRowBackgroundOrDefault(AssetPath, Surface, SearchText, DefaultBackground);
			})
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					Content
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					BuildRowActions(AssetPath, Surface, SearchText)
				]
			];
	}

	static void RegisterRowSearchAliases(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& PrimaryKey,
		const TSharedRef<SWidget>& RowWidget,
		const TCHAR* DebugMode)
	{
		if (PrimaryKey.IsEmpty())
		{
			return;
		}
		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			Surface,
			PrimaryKey,
			RowWidget,
			DebugMode);
		const FString Tail = ExtractReadableTail(PrimaryKey);
		if (!Tail.IsEmpty() && Tail != PrimaryKey)
		{
			FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
				AssetPath,
				Surface,
				Tail,
				RowWidget,
				DebugMode);
		}
	}

	using FDataTableReviewColumnPtr = TSharedPtr<FDataTableEditorColumnHeaderData>;
	using FDataTableReviewRowPtr = TSharedPtr<FDataTableEditorRowListViewData>;

	static const FName ReviewDataTableRowNameColumnId(TEXT("RowName"));
	static const FName ReviewDataTableActionsColumnId(TEXT("ReviewActions"));

	class SBlueprintHelperReviewDataTableRow
		: public SMultiColumnTableRow<FDataTableReviewRowPtr>
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
					BuildRowHighlightShell(
						AssetPath,
						EBlueprintHelperReviewSurface::DataTable,
						SearchText,
						Cell)
				];

			RegisterRowSearchAliases(
				AssetPath,
				EBlueprintHelperReviewSurface::DataTable,
				SearchText,
				RowCell,
				TEXT("native_datatable_row"));
			RegisterRowSearchAliases(
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

			if (ColumnName == ReviewDataTableActionsColumnId)
			{
				return SNew(SSpacer)
					.Size(FVector2D(1.0f, 1.0f));
			}

			if (ColumnName == ReviewDataTableRowNameColumnId)
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

	static TSharedRef<SWidget> BuildAssetSummaryRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& Label,
		const FString& SearchText,
		const FLinearColor& DefaultBackground = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f))
	{
		TSharedRef<SWidget> RowContent =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.Info")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.84f, 0.84f, 1.0f)))
			];

		TSharedRef<SWidget> RowWidget = SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(Surface)
			.TargetKey(SearchText)
			[
				BuildRowHighlightShell(
					AssetPath,
					Surface,
					SearchText,
					RowContent,
					DefaultBackground)
			];
		RegisterRowSearchAliases(AssetPath, Surface, SearchText, RowWidget, TEXT("native_asset_summary_row"));
		return RowWidget;
	}

	static FString GetDetailNodeSearchText(const TSharedPtr<IDetailTreeNode>& DetailNode)
	{
		if (!DetailNode.IsValid())
		{
			return FString();
		}

		TArray<FString> FilterStrings;
		DetailNode->GetFilterStrings(FilterStrings);
		FString SearchText = DetailNode->GetNodeName().ToString();
		for (const FString& FilterString : FilterStrings)
		{
			if (!FilterString.IsEmpty())
			{
				if (!SearchText.IsEmpty())
				{
					SearchText += TEXT(" ");
				}
				SearchText += FilterString;
			}
		}
		if (const TSharedPtr<IPropertyHandle> PropertyHandle = DetailNode->CreatePropertyHandle())
		{
			if (const FProperty* Property = PropertyHandle->GetProperty())
			{
				SearchText += FString::Printf(
					TEXT(" object_property:%s data_asset_property:%s %s"),
					*Property->GetName(),
					*Property->GetName(),
					*Property->GetDisplayNameText().ToString());
			}
		}
		SearchText.TrimStartAndEndInline();
		return SearchText;
	}

	static void FlattenDetailTreeNodes(
		const TArray<TSharedRef<IDetailTreeNode>>& Nodes,
		int32 Depth,
		TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& OutRows)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
		{
			TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row =
				MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
			Row->DetailNode = Node;
			Row->Depth = Depth;
			Row->bIsSection = Node->GetNodeType() == EDetailNodeType::Category
				|| Node->GetNodeType() == EDetailNodeType::Object;
			Row->Label = Node->GetNodeName().ToString();
			Row->SearchText = GetDetailNodeSearchText(Node);
			OutRows.Add(Row);

			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			FlattenDetailTreeNodes(Children, Depth + 1, OutRows);
		}
	}

	static TSharedRef<SWidget> BuildDataAssetRowContent(
		const TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>& Item)
	{
		if (!Item.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		if (Item->DetailNode.IsValid())
		{
			const FNodeWidgets NodeWidgets = Item->DetailNode->CreateNodeWidgets();
			if (NodeWidgets.WholeRowWidget.IsValid())
			{
				return NodeWidgets.WholeRowWidget.ToSharedRef();
			}
			TSharedRef<SWidget> NameWidget = NodeWidgets.NameWidget.IsValid()
				? NodeWidgets.NameWidget.ToSharedRef()
				: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(Item->Label)));
			TSharedRef<SWidget> ValueWidget = NodeWidgets.ValueWidget.IsValid()
				? NodeWidgets.ValueWidget.ToSharedRef()
				: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(Item->Value)));
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.42f)
				.VAlign(VAlign_Center)
				[
					NameWidget
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.58f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					ValueWidget
				];
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Label))
				.ColorAndOpacity(FSlateColor(Item->bIsSection
					? FLinearColor(0.88f, 0.88f, 0.88f, 1.0f)
					: FLinearColor(0.72f, 0.72f, 0.72f, 1.0f)))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Value))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
			];
	}

	static TSharedRef<ITableRow> GenerateDataAssetRow(
		TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable,
		const FString AssetPath,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
	{
		const FString SearchText = Item.IsValid() && !Item->SearchText.IsEmpty()
			? Item->SearchText
			: (Item.IsValid() ? Item->Label : FString(TEXT("data_asset_row")));
		const float DepthPadding = Item.IsValid()
			? static_cast<float>(FMath::Max(0, Item->Depth) * 12)
			: 0.0f;
		const FLinearColor DefaultBackground = Item.IsValid() && Item->bIsSection
			? FLinearColor(0.18f, 0.18f, 0.18f, 1.0f)
			: FLinearColor::Transparent;

		TSharedRef<SWidget> RowContent = SNew(SBlueprintHelperReviewGeometryProbe)
			.Surface(EBlueprintHelperReviewSurface::DataAsset)
			.TargetKey(SearchText)
			.OnGeometryInvalidated(OnGeometryInvalidated)
			[
				BuildRowHighlightShell(
					AssetPath,
					EBlueprintHelperReviewSurface::DataAsset,
					SearchText,
					BuildDataAssetRowContent(Item),
					DefaultBackground)
			];

		TSharedRef<STableRow<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> RowWidget =
			SNew(STableRow<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>, OwnerTable)
			.Padding(FMargin(DepthPadding, 1.0f, 2.0f, 1.0f))
			[
				RowContent
			];

		if (Item.IsValid())
		{
			Item->RowWidget = RowContent;
			RegisterRowSearchAliases(
				AssetPath,
				EBlueprintHelperReviewSurface::DataAsset,
				SearchText,
				RowContent,
				Item->DetailNode.IsValid() ? TEXT("native_details_row") : TEXT("native_structure_row"));
			RegisterRowSearchAliases(
				AssetPath,
				EBlueprintHelperReviewSurface::DataAsset,
				Item->Label,
				RowContent,
				Item->DetailNode.IsValid() ? TEXT("native_details_row") : TEXT("native_structure_row"));
		}

		return RowWidget;
	}
}

bool FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		{TEXT("umg_widget"), TEXT("umg_widget_property"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewWidgetTreePresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	using namespace BlueprintHelperReviewAssetPresentersPrivate;

	State.RootItems.Reset();
	State.TreeView.Reset();

	TArray<FString> PlaceholderLines;
	PlaceholderLines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	PlaceholderLines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Context.Blueprint.Get());
	if (!WidgetBlueprint)
	{
		PlaceholderLines.Add(TEXT("WidgetTree: unavailable"));
		return BuildSummaryPanel(
			TEXT("UMG Widget Tree"),
			PlaceholderLines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		PlaceholderLines.Add(TEXT("WidgetTree: unavailable"));
		return BuildSummaryPanel(
			TEXT("UMG Widget Tree"),
			PlaceholderLines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	TSet<const UWidget*> ReachableWidgets;
	if (WidgetTree->RootWidget)
	{
		if (FWidgetTreeRowItemPtr RootItem = BuildWidgetTreeRowItem(
			WidgetTree->RootWidget,
			0,
			ReachableWidgets))
		{
			State.RootItems.Add(RootItem);
		}
	}

	TArray<FWidgetTreeRowItemPtr> UnparentedItems;
	CollectUnparentedWidgetTreeItems(WidgetTree, ReachableWidgets, UnparentedItems);
	if (UnparentedItems.Num() > 0)
	{
		FWidgetTreeRowItemPtr UnparentedGroup = MakeWidgetTreeRowItem(
			FName(TEXT("Unparented Widgets")),
			FString(),
			0);
		UnparentedGroup->Children = MoveTemp(UnparentedItems);
		State.RootItems.Add(UnparentedGroup);
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<STreeView<FWidgetTreeRowItemPtr>> TreeView =
		SAssignNew(State.TreeView, STreeView<FWidgetTreeRowItemPtr>)
		.TreeItemsSource(&State.RootItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			FWidgetTreeRowItemPtr Item,
			const TSharedRef<STableViewBase>& OwnerTable)
		{
			return GenerateWidgetTreeRow(Item, OwnerTable, AssetPath, OnGeometryInvalidated);
		})
		.OnGetChildren_Lambda([](
			FWidgetTreeRowItemPtr Item,
			TArray<FWidgetTreeRowItemPtr>& OutChildren)
		{
			if (Item.IsValid())
			{
				OutChildren.Append(Item->Children);
			}
		});

	TreeView->RequestTreeRefresh();
	for (const FWidgetTreeRowItemPtr& RootItem : State.RootItems)
	{
		ExpandWidgetTreeRows(State.TreeView, RootItem);
	}

	return SNew(SBorder)
		.Padding(8.0f)
		[
			TreeView
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataTable,
		{TEXT("datatable_row"), TEXT("data_table"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewDataTablePresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	using namespace BlueprintHelperReviewAssetPresentersPrivate;

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
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("DataTable Summary"),
			Lines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	FDataTableEditorUtils::CacheDataTableForEditing(DataTable, State.Columns, State.Rows);

	TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(ReviewDataTableRowNameColumnId)
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
		SHeaderRow::Column(ReviewDataTableActionsColumnId)
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

	const FString AssetName = GetAssetShortName(Context.AssetPath);
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
				BuildAssetSummaryRow(
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

bool FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataAsset,
		{
			TEXT("object_property"),
			TEXT("data_asset_property"),
			TEXT("asset_factory"),
			TEXT("structure"),
			TEXT("struct_field"),
			TEXT("structure_field")
		});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewDataAssetPresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	using namespace BlueprintHelperReviewAssetPresentersPrivate;

	State.Rows.Reset();
	State.ListView.Reset();
	State.PropertyRowGenerator.Reset();

	const FString AssetName = GetAssetShortName(Context.AssetPath);

	if (UUserDefinedStruct* Structure = Context.Structure.Get())
	{
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> SummaryRow =
			MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		SummaryRow->Label = FString::Printf(TEXT("Structure: %s"), *AssetName);
		SummaryRow->Value = FString::Printf(TEXT("Fields: %d"), FStructureEditorUtils::GetVarDesc(Structure).Num());
		SummaryRow->SearchText = FString::Printf(
			TEXT("asset_factory:structure structure %s %s"),
			*AssetName,
			*Context.AssetPath);
		SummaryRow->bIsSection = true;
		State.Rows.Add(SummaryRow);

		for (const FStructVariableDescription& Variable : FStructureEditorUtils::GetVarDesc(Structure))
		{
			const FString FriendlyName = Variable.FriendlyName.IsEmpty()
				? Variable.VarName.ToString()
				: Variable.FriendlyName;
			TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row =
				MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
			Row->Label = FriendlyName;
			Row->Value = Variable.ToPinType().PinCategory.ToString();
			Row->SearchText = FString::Printf(
				TEXT("struct_field:%s structure_field:%s %s"),
				*FriendlyName,
				*FriendlyName,
				*Variable.VarName.ToString());
			Row->Depth = 1;
			State.Rows.Add(Row);
		}
	}
	else if (UObject* AssetObject = Context.AssetObject.Get())
	{
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> SummaryRow =
			MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		SummaryRow->Label = FString::Printf(TEXT("Object: %s"), *AssetName);
		SummaryRow->Value = AssetObject->GetClass() ? AssetObject->GetClass()->GetName() : FString(TEXT("<unknown>"));
		SummaryRow->SearchText = FString::Printf(
			TEXT("asset_factory:data_asset data_asset object_property %s %s"),
			*AssetName,
			*Context.AssetPath);
		SummaryRow->bIsSection = true;
		State.Rows.Add(SummaryRow);

		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FPropertyRowGeneratorArgs RowGeneratorArgs;
		RowGeneratorArgs.bShouldShowHiddenProperties = false;
		RowGeneratorArgs.bAllowEditingClassDefaultObjects = false;
		State.PropertyRowGenerator = PropertyEditorModule.CreatePropertyRowGenerator(RowGeneratorArgs);
		State.PropertyRowGenerator->SetObjects({ AssetObject });

		FlattenDetailTreeNodes(
			State.PropertyRowGenerator->GetRootTreeNodes(),
			0,
			State.Rows);
	}

	if (State.Rows.Num() == 0)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
		Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));
		Lines.Add(TEXT("Object rows: unavailable"));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("Object Details"),
			Lines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView =
		SAssignNew(State.ListView, SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>)
		.ListItemsSource(&State.Rows)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return GenerateDataAssetRow(Item, OwnerTable, AssetPath, OnGeometryInvalidated);
		});

	return SNew(SBorder)
		.Padding(8.0f)
		[
			ListView
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange);
}
