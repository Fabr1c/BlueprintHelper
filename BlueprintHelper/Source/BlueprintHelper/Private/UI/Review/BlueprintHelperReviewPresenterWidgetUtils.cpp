// BlueprintHelper Review presenter widget utility helpers.

#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SPinTypeSelector.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

bool FBlueprintHelperReviewPresenterWidgetUtils::TargetKindEqualsAny(
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

bool FBlueprintHelperReviewPresenterWidgetUtils::ChangeHasTargetKind(
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

bool FBlueprintHelperReviewPresenterWidgetUtils::IsSurfaceRoutable(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(Change, Surface);
}

bool FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
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

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildLine(
	const FString& Text,
	const FLinearColor& Color)
{
	return SNew(STextBlock)
		.ColorAndOpacity(FSlateColor(Color))
		.AutoWrapText(true)
		.Text(FText::FromString(Text));
}

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildRegisteredLine(
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

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
	const FString& Title,
	const TArray<FString>& Lines,
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
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

FString FBlueprintHelperReviewPresenterWidgetUtils::GetAssetShortName(const FString& AssetPath)
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

FString FBlueprintHelperReviewPresenterWidgetUtils::ExtractReadableTail(FString Text)
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

FSlateColor FBlueprintHelperReviewPresenterWidgetUtils::GetRowBackgroundOrDefault(
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

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildRowActions(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	TWeakPtr<SWidget> HoverSource)
{
	return SNew(SHorizontalBox)
		.Visibility_Lambda([AssetPath, Surface, SearchText, HoverSource]()
		{
			const bool bHasDiff = FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
				AssetPath,
				Surface,
				SearchText).GetSpecifiedColor().A > 0.0f;
			const bool bSelected = FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
				AssetPath,
				Surface,
				SearchText) == EVisibility::Visible;
			const TSharedPtr<SWidget> HoverWidget = HoverSource.Pin();
			const bool bHovered = HoverWidget.IsValid() && HoverWidget->IsHovered();
			return bHasDiff && (bHovered || bSelected)
				? EVisibility::Visible
				: EVisibility::Collapsed;
		})
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Accept")))
			.OnClicked_Lambda([AssetPath, Surface, SearchText]()
			{
				return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(AssetPath, Surface, SearchText, EBlueprintHelperReviewActionIntentKind::Accept, TEXT("row_action"));
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Reject")))
			.OnClicked_Lambda([AssetPath, Surface, SearchText]()
			{
				return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(AssetPath, Surface, SearchText, EBlueprintHelperReviewActionIntentKind::Reject, TEXT("row_action"));
			})
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildRowHighlightShell(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	TSharedRef<SWidget> Content,
	const FLinearColor& DefaultBackground,
	const FMargin& Padding)
{
	TSharedPtr<SBorder> RowBorder;
	SAssignNew(RowBorder, SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
		.BorderBackgroundColor_Lambda([AssetPath, Surface, SearchText, DefaultBackground]()
		{
			return GetRowBackgroundOrDefault(AssetPath, Surface, SearchText, DefaultBackground);
		})
		.Padding(Padding);

	RowBorder->SetContent(
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
				BuildRowActions(AssetPath, Surface, SearchText, RowBorder)
			]
		);
	return RowBorder.ToSharedRef();
}

void FBlueprintHelperReviewPresenterWidgetUtils::RegisterRowSearchAliases(
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

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildAssetSummaryRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& Label,
	const FString& SearchText,
	const FLinearColor& DefaultBackground)
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

FString FBlueprintHelperReviewPresenterWidgetUtils::GetWidgetTreeClassText(const UWidget* Widget)
{
	if (!Widget || !Widget->GetClass())
	{
		return TEXT("Unknown");
	}
	return Widget->GetClass()->GetName();
}

TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> FBlueprintHelperReviewPresenterWidgetUtils::MakeWidgetTreeRowItem(
	FName WidgetName,
	const FString& WidgetClass,
	int32 Depth)
{
	TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item = MakeShared<FBlueprintHelperReviewWidgetTreeRowItem>();
	Item->WidgetName = WidgetName;
	Item->WidgetClass = WidgetClass;
	Item->Depth = Depth;
	return Item;
}

TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> FBlueprintHelperReviewPresenterWidgetUtils::BuildWidgetTreeRowItem(
	UWidget* Widget,
	int32 Depth,
	TSet<const UWidget*>& ReachableWidgets)
{
	if (!Widget || ReachableWidgets.Contains(Widget))
	{
		return nullptr;
	}

	ReachableWidgets.Add(Widget);
	TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item = MakeWidgetTreeRowItem(
		Widget->GetFName(),
		GetWidgetTreeClassText(Widget),
		Depth);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = Panel->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> ChildItem = BuildWidgetTreeRowItem(
				Panel->GetChildAt(ChildIndex),
				Depth + 1,
				ReachableWidgets);
			if (ChildItem.IsValid())
			{
				Item->Children.Add(ChildItem);
			}
		}
	}

	return Item;
}

void FBlueprintHelperReviewPresenterWidgetUtils::CollectUnparentedWidgetTreeItems(
	UWidgetTree* WidgetTree,
	const TSet<const UWidget*>& ReachableWidgets,
	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>& OutItems)
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

bool FBlueprintHelperReviewPresenterWidgetUtils::IsWidgetTreeGroupRow(
	const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item)
{
	return Item.IsValid()
		&& Item->WidgetName == FName(TEXT("Unparented Widgets"))
		&& Item->WidgetClass.IsEmpty();
}

void FBlueprintHelperReviewPresenterWidgetUtils::RegisterWidgetTreeRowAliases(
	const FString& AssetPath,
	const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item,
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

TSharedRef<ITableRow> FBlueprintHelperReviewPresenterWidgetUtils::GenerateWidgetTreeRow(
	TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable,
	const FString& AssetPath,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	const FString WidgetName = Item.IsValid() ? Item->WidgetName.ToString() : FString(TEXT("<invalid>"));
	const FString WidgetClass = Item.IsValid() ? Item->WidgetClass : FString();
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
							return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(AssetPath, EBlueprintHelperReviewSurface::UMGWidgetTree, ProbeTargetKey, EBlueprintHelperReviewActionIntentKind::Accept, TEXT("row_action"));
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Reject")))
						.OnClicked_Lambda([AssetPath, ProbeTargetKey]()
						{
							return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(AssetPath, EBlueprintHelperReviewSurface::UMGWidgetTree, ProbeTargetKey, EBlueprintHelperReviewActionIntentKind::Reject, TEXT("row_action"));
						})
					]
				]
			]
		];

	TSharedRef<STableRow<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>> RowWidget =
		SNew(STableRow<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>, OwnerTable)
		.Padding(FMargin(DepthPadding, 2.0f, 4.0f, 2.0f))
		[
			RowContent
		];

	if (Item.IsValid())
	{
		Item->RowWidget = RowContent;
	}
	RegisterWidgetTreeRowAliases(AssetPath, Item, RowContent);
	return RowWidget;
}

void FBlueprintHelperReviewPresenterWidgetUtils::ExpandWidgetTreeRows(
	const TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>>& TreeView,
	const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item)
{
	if (!TreeView.IsValid() || !Item.IsValid())
	{
		return;
	}

	TreeView->SetItemExpansion(Item, true);
	for (const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Child : Item->Children)
	{
		ExpandWidgetTreeRows(TreeView, Child);
	}
}

const FName& FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableRowNameColumnId()
{
	static const FName ColumnId(TEXT("RowName"));
	return ColumnId;
}

const FName& FBlueprintHelperReviewPresenterWidgetUtils::GetDataTableActionsColumnId()
{
	static const FName ColumnId(TEXT("ReviewActions"));
	return ColumnId;
}

FString FBlueprintHelperReviewPresenterWidgetUtils::GetDetailNodeSearchText(
	const TSharedPtr<IDetailTreeNode>& DetailNode)
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

void FBlueprintHelperReviewPresenterWidgetUtils::FlattenDetailTreeNodes(
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

TSharedRef<SWidget> FBlueprintHelperReviewPresenterWidgetUtils::BuildDataAssetRowContent(
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
			return SNew(SBox)
				.IsEnabled(false)
				[
					NodeWidgets.WholeRowWidget.ToSharedRef()
				];
		}
		TSharedRef<SWidget> NameWidget = NodeWidgets.NameWidget.IsValid()
			? NodeWidgets.NameWidget.ToSharedRef()
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(Item->Label)));
		TSharedRef<SWidget> ValueWidget = NodeWidgets.ValueWidget.IsValid()
			? NodeWidgets.ValueWidget.ToSharedRef()
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(Item->Value)));
		return SNew(SBox)
			.IsEnabled(false)
			[
				SNew(SHorizontalBox)
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
				]
			];
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	const FText VariableTypeText = Item->bHasPinType
		? UEdGraphSchema_K2::TypeToText(Item->PinType)
		: FText::GetEmpty();
	TSharedRef<SWidget> VariableTypeIcon = SNew(SSpacer);
	if (Item->bHasPinType && K2Schema)
	{
		const FSlateBrush* PrimaryIcon = FBlueprintEditorUtils::GetIconFromPin(Item->PinType);
		const FSlateColor PrimaryColor = K2Schema->GetPinTypeColor(Item->PinType);
		const FSlateBrush* SecondaryIcon = FBlueprintEditorUtils::GetSecondaryIconFromPin(Item->PinType);
		const FSlateColor SecondaryColor = K2Schema->GetSecondaryPinTypeColor(Item->PinType);
		VariableTypeIcon = SPinTypeSelector::ConstructPinTypeImage(
			PrimaryIcon,
			PrimaryColor,
			SecondaryIcon,
			SecondaryColor,
			TSharedPtr<SToolTip>());
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
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			.Visibility(Item->bHasPinType ? EVisibility::Visible : EVisibility::Collapsed)
			.ToolTipText(VariableTypeText)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VariableTypeIcon
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(VariableTypeText)
			]
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

TSharedRef<ITableRow> FBlueprintHelperReviewPresenterWidgetUtils::GenerateDataAssetRow(
	TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable,
	const FString& AssetPath,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated,
	EBlueprintHelperReviewSurface Surface,
	const FMargin& HighlightPadding)
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
		.Surface(Surface)
		.TargetKey(SearchText)
		.OnGeometryInvalidated(OnGeometryInvalidated)
		[
			BuildRowHighlightShell(
				AssetPath,
				Surface,
				SearchText,
				BuildDataAssetRowContent(Item),
				DefaultBackground,
				HighlightPadding)
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
			Surface,
			SearchText,
			RowContent,
			Item->DetailNode.IsValid() ? TEXT("native_details_row") : TEXT("native_structure_row"));
		RegisterRowSearchAliases(
			AssetPath,
			Surface,
			Item->Label,
			RowContent,
			Item->DetailNode.IsValid() ? TEXT("native_details_row") : TEXT("native_structure_row"));
	}

	return RowWidget;
}


