// BlueprintHelper Review asset-specific presenters.

#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"

#include <initializer_list>

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Engine/DataTable.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "WidgetBlueprint.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
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
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
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
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	FBlueprintHelperDataTableService DataTableService;
	const FString DataTableAssetPath = Context.ObjectPath.IsEmpty() ? Context.AssetPath : Context.ObjectPath;
	const FBlueprintHelperDataTableRowsResult Result = DataTableService.GetDataTableRows(DataTableAssetPath);
	if (!Result.bSuccess)
	{
		Lines.Add(FString::Printf(TEXT("DataTable: unavailable (%s)"), *Result.ErrorMessage));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("DataTable Summary"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::DataTable,
			OnGeometryInvalidated);
	}

	Lines.Add(FString::Printf(TEXT("Row struct: %s"), Result.RowStructName.IsEmpty() ? TEXT("<none>") : *Result.RowStructName));
	Lines.Add(FString::Printf(TEXT("Column count: %d"), Result.Columns.Num()));
	Lines.Add(FString::Printf(TEXT("Row count: %d"), Result.Rows.Num()));

	for (const FBlueprintHelperDataTableColumnInfo& Column : Result.Columns)
	{
		Lines.Add(FString::Printf(TEXT("Column: %s : %s"), *Column.Name, *Column.TypeName));
	}

	for (const FBlueprintHelperDataTableRowInfo& Row : Result.Rows)
	{
		FString RowLine = FString::Printf(TEXT("Row: %s"), *Row.RowName.ToString());
		for (const TPair<FString, FString>& Field : Row.Fields)
		{
			RowLine += FString::Printf(TEXT(" %s=%s"), *Field.Key, *Field.Value);
		}
		Lines.Add(RowLine);
	}

	return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
		TEXT("DataTable Summary"),
		Lines,
		Context.AssetPath,
		EBlueprintHelperReviewSurface::DataTable,
		OnGeometryInvalidated);
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
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
		{TEXT("object_property"), TEXT("data_asset_property"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	FBlueprintHelperPropertyReflectionService PropertyService;
	const FString ObjectAssetPath = Context.ObjectPath.IsEmpty() ? Context.AssetPath : Context.ObjectPath;
	const FBlueprintHelperObjectPropertiesResult Result = PropertyService.GetObjectProperties(ObjectAssetPath);
	if (!Result.bSuccess)
	{
		Lines.Add(FString::Printf(TEXT("Object properties: unavailable (%s)"), *Result.ErrorMessage));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("Object Details Summary"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::DataAsset,
			OnGeometryInvalidated);
	}

	Lines.Add(FString::Printf(TEXT("Class: %s"), Result.ClassName.IsEmpty() ? TEXT("<none>") : *Result.ClassName));
	Lines.Add(FString::Printf(TEXT("Property count: %d"), Result.Properties.Num()));
	for (const FBlueprintHelperObjectPropertyInfo& Property : Result.Properties)
	{
		Lines.Add(FString::Printf(
			TEXT("- %s : %s = %s"),
			*Property.Name,
			*Property.TypeName,
			*Property.Value));
	}

	return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
		TEXT("Object Details Summary"),
		Lines,
		Context.AssetPath,
		EBlueprintHelperReviewSurface::DataAsset,
		OnGeometryInvalidated);
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange);
}
