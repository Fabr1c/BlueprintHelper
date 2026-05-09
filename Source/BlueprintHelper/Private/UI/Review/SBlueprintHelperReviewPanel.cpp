// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "SKismetInspector.h"
#include "SMyBlueprint.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class FSBlueprintHelperReviewPanelLocalUtils
{
public:
	inline static const FLinearColor ReviewGreen = FLinearColor(0.05f, 0.75f, 0.22f, 0.85f);
	inline static const FLinearColor ReviewRed = FLinearColor(0.95f, 0.12f, 0.10f, 0.85f);
	inline static const FLinearColor ReviewYellow = FLinearColor(1.0f, 0.72f, 0.08f, 0.85f);
	inline static const FLinearColor ReviewPanelBg = FLinearColor(0.015f, 0.015f, 0.015f, 1.0f);
	inline static const FLinearColor ReviewSectionBg = FLinearColor(0.035f, 0.035f, 0.035f, 1.0f);

	static FText StatusToText(EBlueprintHelperReviewChangeStatus Status)
	{
		return FText::FromString(BlueprintHelperReviewChangeStatusToString(Status));
	}
};

void SBlueprintHelperReviewPanel::Construct(const FArguments& InArgs)
{
	ReviewStoreService = InArgs._ReviewStoreService;
	ReviewActionService = InArgs._ReviewActionService;

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges = InArgs._InitialChanges;
	if (InitialChanges.Num() == 0 && ReviewStoreService)
	{
		InitialChanges = ReviewStoreService->LoadPendingVisibleChanges();
	}
	RefreshVisibleChanges(InitialChanges);
	if (ChangeItems.Num() > 0)
	{
		SelectedChange = ChangeItems[0];
	}
	LoadReviewAssetFromSelection();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewPanelBg)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				+ SSplitter::Slot()
				.Value(0.14f)
				[
					BuildFinalChangeSidebar()
				]
				+ SSplitter::Slot()
				.Value(0.86f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Horizontal)
					+ SSplitter::Slot()
					.Value(0.18f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.42f)
						.Padding(4.0f)
						[
							BuildComponentsPanel()
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.58f)
						.Padding(4.0f)
						[
							BuildMyBlueprintPanel()
						]
					]
					+ SSplitter::Slot()
					.Value(0.62f)
					[
						BuildGraphPanel()
					]
					+ SSplitter::Slot()
					.Value(0.20f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.76f)
						[
							BuildDetailsPanel()
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.24f)
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							BuildDebugPanel()
						]
					]
				]
			]
		]
	];

	RefreshChangeTreeWidget();
	UpdateDetailsSelection();
	RequestDeferredDiffGeometryRefresh();
}

void SBlueprintHelperReviewPanel::RefreshVisibleChanges(
	const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges)
{
	ChangeItems.Empty();
	for (const FBlueprintHelperReviewVisibleChange& Change : SourceChanges)
	{
		ChangeItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	}
	RebuildChangeTreeItems();
}

void SBlueprintHelperReviewPanel::OnChangeSelectionChanged(FReviewChangeItem Item, ESelectInfo::Type SelectInfo)
{
	SelectedChange = Item;
	LoadReviewAssetFromSelection();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildGraphEditorWidget());
	}
	RefreshDiffStackWidgets();
	UpdateDetailsSelection();
	RequestDeferredDiffGeometryRefresh();
	StartFlash();
	if (Item.IsValid())
	{
		AddDebugMessage(FString::Printf(
			TEXT("Selected change id=%s label=\"%s\" asset=\"%s\" graph=\"%s\" latest=%s"),
			*Item->ChangeId,
			*Item->DisplayLabel,
			*Item->AssetPath,
			*Item->GraphName,
			*Item->LatestTransactionId));
	}
	if (SelectInfo != ESelectInfo::Direct)
	{
		RefreshChangeTreeWidget();
	}
}

void SBlueprintHelperReviewPanel::AddDebugMessage(const FString& Message)
{
	static constexpr int32 MaxDebugMessages = 200;

	DebugMessages.Insert(FString::Printf(
		TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		*Message), 0);
	if (DebugMessages.Num() > MaxDebugMessages)
	{
		DebugMessages.SetNum(MaxDebugMessages);
	}

	if (DebugMessageTextBox.IsValid())
	{
		DebugMessageTextBox->SetText(GetDebugMessagesText());
	}
}

FString SBlueprintHelperReviewPanel::BuildDebugMessagesString() const
{
	return FBlueprintHelperReviewDebugText::BuildCopyableText(DebugMessages);
}

FText SBlueprintHelperReviewPanel::GetDebugMessagesText() const
{
	return FText::FromString(BuildDebugMessagesString());
}

FReply SBlueprintHelperReviewPanel::OnCopyDebugMessages() const
{
	const FString DebugText = BuildDebugMessagesString();
	FPlatformApplicationMisc::ClipboardCopy(*DebugText);
	return FReply::Handled();
}

TSharedRef<ITableRow> SBlueprintHelperReviewPanel::GenerateChangeTreeRow(
	FReviewTreeItemPtr Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item.IsValid() && Item->bIsAssetRoot)
	{
		return SNew(STableRow<FReviewTreeItemPtr>, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
			.Padding(7.0f, 5.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Item->AssetPath))
			]
		];
	}

	return SNew(STableRow<FReviewTreeItemPtr>, OwnerTable)
	.Padding(0.0f)
	[
		BuildDiffRow(Item.IsValid() ? Item->Change : FReviewChangeItem(), true)
	];
}

void SBlueprintHelperReviewPanel::GetChangeTreeChildren(
	FReviewTreeItemPtr Item,
	TArray<FReviewTreeItemPtr>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}

void SBlueprintHelperReviewPanel::OnChangeTreeSelectionChanged(
	FReviewTreeItemPtr Item,
	ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}

	if (Item->bIsAssetRoot)
	{
		if (ChangeTreeView.IsValid())
		{
			ChangeTreeView->SetItemExpansion(Item, true);
		}
		if (Item->Children.Num() > 0)
		{
			OnChangeTreeSelectionChanged(Item->Children[0], SelectInfo);
		}
		return;
	}

	if (Item->Change.IsValid())
	{
		OnChangeSelectionChanged(Item->Change, ESelectInfo::Direct);
	}
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildFinalChangeSidebar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewSectionBg)
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u6700\u7ec8\u6539\u52a8")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ChangeTreeView, STreeView<FReviewTreeItemPtr>)
				.TreeItemsSource(&ChangeTreeRootItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SBlueprintHelperReviewPanel::GenerateChangeTreeRow)
				.OnGetChildren(this, &SBlueprintHelperReviewPanel::GetChangeTreeChildren)
				.OnSelectionChanged(this, &SBlueprintHelperReviewPanel::OnChangeTreeSelectionChanged)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				BuildAssetChangeButtonBar()
			]
		];
}

void SBlueprintHelperReviewPanel::RebuildChangeTreeItems()
{
	ChangeTreeRootItems.Reset();

	TMap<FString, FReviewTreeItemPtr> AssetRootsByPath;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const FString AssetPath = Item->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Item->AssetPath;
		FReviewTreeItemPtr* ExistingRoot = AssetRootsByPath.Find(AssetPath);
		if (!ExistingRoot)
		{
			FReviewTreeItemPtr Root = MakeShared<FReviewTreeItem>();
			Root->bIsAssetRoot = true;
			Root->AssetPath = AssetPath;
			AssetRootsByPath.Add(AssetPath, Root);
			ChangeTreeRootItems.Add(Root);
			ExistingRoot = AssetRootsByPath.Find(AssetPath);
		}

		FReviewTreeItemPtr Leaf = MakeShared<FReviewTreeItem>();
		Leaf->AssetPath = AssetPath;
		Leaf->Change = Item;
		(*ExistingRoot)->Children.Add(Leaf);
	}
}

void SBlueprintHelperReviewPanel::RefreshChangeTreeWidget()
{
	if (!ChangeTreeView.IsValid())
	{
		return;
	}

	ChangeTreeView->RequestTreeRefresh();
	for (const FReviewTreeItemPtr& Root : ChangeTreeRootItems)
	{
		if (Root.IsValid())
		{
			ChangeTreeView->SetItemExpansion(Root, true);
		}
	}

	if (SelectedChange.IsValid())
	{
		if (FReviewTreeItemPtr TreeItem = FindTreeItemForChange(SelectedChange))
		{
			ChangeTreeView->SetSelection(TreeItem);
		}
	}
}

SBlueprintHelperReviewPanel::FReviewTreeItemPtr SBlueprintHelperReviewPanel::FindTreeItemForChange(
	FReviewChangeItem Item) const
{
	if (!Item.IsValid())
	{
		return nullptr;
	}

	for (const FReviewTreeItemPtr& Root : ChangeTreeRootItems)
	{
		if (!Root.IsValid())
		{
			continue;
		}
		for (const FReviewTreeItemPtr& Child : Root->Children)
		{
			if (Child.IsValid() && Child->Change == Item)
			{
				return Child;
			}
		}
	}
	return nullptr;
}


TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildComponentsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewSectionBg)
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u7ec4\u4ef6")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(ComponentsContentBox, SBox)
					[
						BuildReadonlyComponentsWidget()
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(24.0f, 42.0f, 10.0f, 0.0f)
				[
					SAssignNew(ComponentsDiffStackBox, SBox)
					[
						BuildPanelDiffFrames(
							&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange,
							EBlueprintHelperReviewSurface::Components)
					]
				]
			]
		];
}


TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildMyBlueprintPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewSectionBg)
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u6211\u7684\u84dd\u56fe")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(MyBlueprintContentBox, SBox)
					[
						BuildReadonlyMyBlueprintWidget()
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(6.0f, 34.0f, 6.0f, 0.0f)
				[
					SAssignNew(MyBlueprintDiffStackBox, SBox)
					[
						BuildPanelDiffFrames(
							&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
							EBlueprintHelperReviewSurface::MyBlueprint)
					]
				]
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildGraphPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f))
		.Padding(0.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(GraphEditorBox, SBox)
				[
					BuildGraphEditorWidget()
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 0.0f, 16.0f)
			[
				BuildActionButtonBar()
			]
		];
}


TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDetailsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewSectionBg)
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u7ec6\u8282")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(DetailsContentBox, SBox)
					[
						BuildReadonlyDetailsWidget()
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(6.0f, 34.0f, 6.0f, 0.0f)
				[
					SAssignNew(DetailsDiffStackBox, SBox)
					[
						BuildDetailsPanelDiffFrames()
					]
				]
			]
		];
}


TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDebugPanel()
{
	if (DebugMessages.Num() == 0)
	{
		DebugMessages.Add(TEXT("[init] Review debug panel ready."));
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewSectionBg)
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Debug")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("CopyAll")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnCopyDebugMessages)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f, 0.0f, 8.0f, 8.0f)
			[
				SAssignNew(DebugMessageTextBox, SMultiLineEditableTextBox)
				.Text(this, &SBlueprintHelperReviewPanel::GetDebugMessagesText)
				.IsReadOnly(true)
				.AllowContextMenu(true)
				.AlwaysShowScrollbars(true)
				.AutoWrapText(false)
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ForegroundColor(FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f)))
			]
		];
}


TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildGraphEditorWidget()
{
	FBlueprintHelperReviewGraphPresenterArgs Args;
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SelectedChange = SelectedChange;
	Args.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Args.OnAcceptChangeId = [this](const FString& ChangeId)
	{
		return OnAcceptChangeId(ChangeId);
	};
	Args.OnRejectChangeId = [this](const FString& ChangeId)
	{
		return OnRejectChangeId(ChangeId);
	};
	Args.GetChangeColor = [this](EBlueprintHelperReviewChangeKind Kind)
	{
		return GetChangeColor(Kind);
	};

	return FBlueprintHelperReviewGraphPresenter::BuildContent(Args, GraphPresenterState);
}

SBlueprintHelperReviewPanel::FReviewChangeItem SBlueprintHelperReviewPanel::FindChangeItemById(
	const FString& ChangeId) const
{
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid() && Item->ChangeId == ChangeId)
		{
			return Item;
		}
	}
	return FReviewChangeItem();
}

FReply SBlueprintHelperReviewPanel::OnAcceptChangeId(const FString& ChangeId)
{
	return OnAcceptChange(FindChangeItemById(ChangeId));
}

FReply SBlueprintHelperReviewPanel::OnRejectChangeId(const FString& ChangeId)
{
	return OnRejectChange(FindChangeItemById(ChangeId));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildActionButtonBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("AcceptAll")))
				.OnClicked(this, &SBlueprintHelperReviewPanel::OnAcceptAll)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("RejectAll")))
				.OnClicked(this, &SBlueprintHelperReviewPanel::OnRejectAll)
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyComponentsWidget()
{
	return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(ReviewAssetContext);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyMyBlueprintWidget()
{
	return FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(
		ReviewAssetContext,
		MyBlueprintWidget,
		KismetInspector);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyDetailsWidget()
{
	switch (ResolveDetailsSurfaceFromSelectedChange())
	{
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		KismetInspector.Reset();
		return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(ReviewAssetContext);
	case EBlueprintHelperReviewSurface::DataTable:
		KismetInspector.Reset();
		return FBlueprintHelperReviewDataTablePresenter::BuildContent(ReviewAssetContext);
	case EBlueprintHelperReviewSurface::DataAsset:
		KismetInspector.Reset();
		return FBlueprintHelperReviewDataAssetPresenter::BuildContent(ReviewAssetContext);
	default:
		break;
	}

	return FBlueprintHelperReviewObjectDetailsPresenter::BuildContent(
		ReviewAssetContext,
		KismetInspector,
		MyBlueprintWidget);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildScopedDiffStack(
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	TSharedRef<SScrollBox> Stack = SNew(SScrollBox);
	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	int32 VisibleRowCount = 0;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || !Predicate(*Item))
		{
			continue;
		}
		if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		Stack->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildDiffRow(Item, true)
		];
		++VisibleRowCount;
	}

	if (VisibleRowCount == 0)
	{
		return SNullWidget::NullWidget;
	}

	return Stack;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildPanelDiffFrames(
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&),
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SelectedChange = SelectedChange;
	Args.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Args.OnAcceptChange = [this](FReviewChangeItem Item)
	{
		return OnAcceptChange(Item);
	};
	Args.OnRejectChange = [this](FReviewChangeItem Item)
	{
		return OnRejectChange(Item);
	};
	Args.GetChangeColor = [this](EBlueprintHelperReviewChangeKind Kind)
	{
		return GetChangeColor(Kind);
	};
	Args.GetSelectedDiffColor = [this]()
	{
		return GetSelectedDiffColor();
	};
	Args.ResolveRowGeometry.BindLambda([this](
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface RoutedSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		return ResolveReviewRowGeometry(Change, RoutedSurface, OutAnchor);
	});

	if (Surface == EBlueprintHelperReviewSurface::Components
		&& Predicate == &FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::MyBlueprint
		&& Predicate == &FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::Details
		&& Predicate == &FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::UMGWidgetTree
		&& Predicate == &FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::DataTable
		&& Predicate == &FBlueprintHelperReviewDataTablePresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewDataTablePresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::DataAsset
		&& Predicate == &FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(Args);
	}
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDetailsPanelDiffFrames()
{
	switch (ResolveDetailsSurfaceFromSelectedChange())
	{
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::UMGWidgetTree);
	case EBlueprintHelperReviewSurface::DataTable:
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::DataTable);
	case EBlueprintHelperReviewSurface::DataAsset:
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::DataAsset);
	default:
		break;
	}

	return BuildPanelDiffFrames(
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange,
		EBlueprintHelperReviewSurface::Details);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildAssetChangeButtonBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(6.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("AcceptAllAssetChange")))
				.OnClicked(this, &SBlueprintHelperReviewPanel::OnAcceptAll)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("RejectAllAssetChange")))
				.OnClicked(this, &SBlueprintHelperReviewPanel::OnRejectAll)
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDiffRow(FReviewChangeItem Item, bool bShowActions)
{
	const FString Label = Item.IsValid() ? Item->DisplayLabel : TEXT("Invalid Change");
	const FString SubLabel = Item.IsValid()
		? FString::Printf(TEXT("%s  %s"),
			*FString(BlueprintHelperReviewChangeKindToString(Item->ChangeKind)),
			*Item->LatestTransactionId)
		: TEXT("");

	TSharedRef<SWidget> Content = SNew(SButton)
		.ContentPadding(6.0f)
		.OnClicked_Lambda([this, Item]()
		{
			OnChangeSelectionChanged(Item, ESelectInfo::OnMouseClick);
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.62f, 1.0f)))
				.Text(FText::FromString(SubLabel))
			]
		];

	return BuildDiffFrame(Item, Content, bShowActions);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDiffFrame(
	FReviewChangeItem Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
		Item,
		Content,
		bShowActions,
		true,
		Item.IsValid() ? GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent),
		[this](FReviewChangeItem ChangeItem)
		{
			return OnAcceptChange(ChangeItem);
		},
		[this](FReviewChangeItem ChangeItem)
		{
			return OnRejectChange(ChangeItem);
		});
}

FReply SBlueprintHelperReviewPanel::OnAcceptSelected()
{
	return OnAcceptChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnRejectSelected()
{
	return OnRejectChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnAcceptChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	SelectedChange = Item;

	FBlueprintHelperReviewActionResult Result;
	if (ReviewActionService)
	{
		Result = ReviewActionService->AcceptVisibleChange(*Item);
	}
	else
	{
		Result.bSucceeded = true;
		Result.TargetTransactionId = Item->LatestTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		Result.Message = TEXT("Accepted visible change.");
	}

	if (Result.bSucceeded)
	{
		const int32 RemovedIndex = ChangeItems.IndexOfByKey(Item);
		ChangeItems.Remove(Item);
		if (ChangeItems.Num() > 0)
		{
			const int32 NextIndex = FMath::Clamp(RemovedIndex, 0, ChangeItems.Num() - 1);
			SelectedChange = ChangeItems[NextIndex];
		}
		else
		{
			SelectedChange.Reset();
		}
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->SetContent(BuildGraphEditorWidget());
		}
		RefreshDiffStackWidgets();
		UpdateDetailsSelection();
	}

	AddDebugMessage(FString::Printf(
		TEXT("Accept change id=%s success=%d message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		*Result.Message));
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnRejectChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	SelectedChange = Item;

	FBlueprintHelperReviewActionResult Result;
	if (ReviewActionService)
	{
		Result = ReviewActionService->RejectVisibleChange(*Item);
	}
	else
	{
		Result.bSucceeded = false;
		Result.TargetTransactionId = Item->LatestTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.Message = TEXT("Reject requires archive-baseline rollback service.");
	}

	Item->Status = Result.NewStatus;
	Item->NeedsActionReason = Result.Message;
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildGraphEditorWidget());
	}
	UpdateDetailsSelection();

	AddDebugMessage(FString::Printf(
		TEXT("Reject change id=%s success=%d status=%d message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		*BlueprintHelperReviewChangeStatusToString(Result.NewStatus),
		*Result.Message));
	return FReply::Handled();
}


FReply SBlueprintHelperReviewPanel::OnAcceptAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	TArray<FReviewChangeItem> AcceptedItems;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || (!AssetPath.IsEmpty() && Item->AssetPath != AssetPath))
		{
			continue;
		}

		FBlueprintHelperReviewActionResult Result;
		if (ReviewActionService)
		{
			Result = ReviewActionService->AcceptVisibleChange(*Item);
		}
		else
		{
			Result.bSucceeded = true;
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (Result.bSucceeded)
		{
			AcceptedItems.Add(Item);
		}
		else
		{
			Item->Status = Result.NewStatus;
			Item->NeedsActionReason = Result.Message;
		}
	}

	ChangeItems.RemoveAll([&AcceptedItems](const FReviewChangeItem& Item)
	{
		return Item.IsValid() && AcceptedItems.Contains(Item);
	});

	SelectedChange = ChangeItems.Num() > 0 ? ChangeItems[0] : FReviewChangeItem();
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildGraphEditorWidget());
	}
	UpdateDetailsSelection();
	AddDebugMessage(FString::Printf(
		TEXT("AcceptAll asset=\"%s\" remainingVisibleChanges=%d"),
		*AssetPath,
		ChangeItems.Num()));
	return FReply::Handled();
}


FReply SBlueprintHelperReviewPanel::OnRejectAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid() && (AssetPath.IsEmpty() || Item->AssetPath == AssetPath))
		{
			FBlueprintHelperReviewActionResult Result;
			if (ReviewActionService)
			{
				Result = ReviewActionService->RejectVisibleChange(*Item);
			}
			else
			{
				Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
				Result.Message = TEXT("Reject requires archive-baseline rollback service.");
			}
			Item->Status = Result.NewStatus;
			Item->NeedsActionReason = Result.Message;
		}
	}

	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildGraphEditorWidget());
	}
	UpdateDetailsSelection();
	AddDebugMessage(FString::Printf(
		TEXT("RejectAll asset=\"%s\" markedNeedsAction=1"),
		*AssetPath));
	return FReply::Handled();
}

FText SBlueprintHelperReviewPanel::GetSelectedTitle() const
{
	if (!SelectedChange.IsValid())
	{
		return FText::FromString(TEXT("No visible change selected."));
	}
	return FText::FromString(SelectedChange->DisplayLabel);
}

FText SBlueprintHelperReviewPanel::GetSelectedBefore() const
{
	return SelectedChange.IsValid()
		? FText::FromString(FString::Printf(TEXT("Before: %s"), *SelectedChange->BeforeSummary))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedAfter() const
{
	return SelectedChange.IsValid()
		? FText::FromString(FString::Printf(TEXT("After: %s"), *SelectedChange->AfterSummary))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedStatus() const
{
	return SelectedChange.IsValid()
		? FText::Format(FText::FromString(TEXT("Status: {0}")), FSBlueprintHelperReviewPanelLocalUtils::StatusToText(SelectedChange->Status))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedTransactionChain() const
{
	if (!SelectedChange.IsValid())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(
		TEXT("Latest: %s\nSources: %s\nNeeds action: %s"),
		*SelectedChange->LatestTransactionId,
		*FString::Join(SelectedChange->SourceTransactionIds, TEXT(", ")),
		*SelectedChange->NeedsActionReason));
}

FSlateColor SBlueprintHelperReviewPanel::GetSelectedDiffColor() const
{
	if (!SelectedChange.IsValid())
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	FLinearColor Color = GetChangeColor(SelectedChange->ChangeKind).GetSpecifiedColor();
	if (FlashAlpha > 0.0f)
	{
		Color.A = FMath::Clamp(0.35f + FlashAlpha * 0.60f, 0.35f, 0.95f);
	}
	return FSlateColor(Color);
}

FSlateColor SBlueprintHelperReviewPanel::GetChangeColor(EBlueprintHelperReviewChangeKind Kind) const
{
	const FString ColorName = BlueprintHelperReviewChangeKindToColorName(Kind);
	if (ColorName == TEXT("green"))
	{
		return FSlateColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewGreen);
	}
	if (ColorName == TEXT("red"))
	{
		return FSlateColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewRed);
	}
	if (ColorName == TEXT("yellow"))
	{
		return FSlateColor(FSBlueprintHelperReviewPanelLocalUtils::ReviewYellow);
	}
	return FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 0.85f));
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickFlash(double InCurrentTime, float InDeltaTime)
{
	FlashAlpha = FMath::Max(0.0f, FlashAlpha - InDeltaTime * 1.8f);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FlashAlpha > 0.0f ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::StartFlash()
{
	FlashAlpha = 1.0f;
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickFlash));
}

void SBlueprintHelperReviewPanel::RequestDeferredDiffGeometryRefresh()
{
	if (bPendingDiffGeometryRefresh)
	{
		return;
	}

	bPendingDiffGeometryRefresh = true;
	RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::RefreshDiffGeometryAfterLayout));
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::RefreshDiffGeometryAfterLayout(double InCurrentTime, float InDeltaTime)
{
	bPendingDiffGeometryRefresh = false;
	RefreshDiffStackWidgets();
	return EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::RefreshDiffStackWidgets()
{
	if (ComponentsDiffStackBox.IsValid())
	{
		ComponentsDiffStackBox->SetContent(BuildPanelDiffFrames(
			&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::Components));
	}
	if (MyBlueprintDiffStackBox.IsValid())
	{
		MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
			&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::MyBlueprint));
	}
	if (DetailsDiffStackBox.IsValid())
	{
		DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SBlueprintHelperReviewPanel::LoadReviewAssetFromSelection()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}
	if (AssetPath.IsEmpty())
	{
		for (const FReviewChangeItem& Item : ChangeItems)
		{
			if (Item.IsValid() && !Item->AssetPath.IsEmpty())
			{
				AssetPath = Item->AssetPath;
				break;
			}
		}
	}

	FBlueprintHelperReviewAssetContext LoadedContext =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(AssetPath);
	const EBlueprintHelperReviewSurface LoadedDetailsSurface = ResolveDetailsSurfaceFromSelectedChange();
	if (LoadedContext.AssetPath == ReviewAssetContext.AssetPath
		&& LoadedContext.AssetKind == ReviewAssetContext.AssetKind
		&& LoadedContext.AssetObject.Get() == ReviewAssetContext.AssetObject.Get()
		&& LoadedContext.Blueprint.Get() == ReviewAssetContext.Blueprint.Get()
		&& LoadedDetailsSurface == DetailsSurface)
	{
		return;
	}

	ReviewAssetContext = LoadedContext;
	DetailsSurface = LoadedDetailsSurface;
	AddDebugMessage(FString::Printf(
		TEXT("ReviewAssetContext asset=\"%s\" package=\"%s\" object=\"%s\" kind=%s valid=%d blueprint=%d detailsSurface=%s"),
		*ReviewAssetContext.AssetPath,
		*ReviewAssetContext.PackageName,
		*ReviewAssetContext.ObjectPath,
		BlueprintHelperReviewAssetKindToString(ReviewAssetContext.AssetKind),
		ReviewAssetContext.IsValid() ? 1 : 0,
		ReviewAssetContext.Blueprint.IsValid() ? 1 : 0,
		BlueprintHelperReviewSurfaceToString(DetailsSurface)));
	if (ComponentsContentBox.IsValid())
	{
		ComponentsContentBox->SetContent(BuildReadonlyComponentsWidget());
	}
	if (MyBlueprintContentBox.IsValid())
	{
		MyBlueprintContentBox->SetContent(BuildReadonlyMyBlueprintWidget());
	}
	if (DetailsContentBox.IsValid())
	{
		DetailsContentBox->SetContent(BuildReadonlyDetailsWidget());
	}
	if (KismetInspector.IsValid())
	{
		if (MyBlueprintWidget.IsValid())
		{
			MyBlueprintWidget->SetInspector(KismetInspector);
		}
	}
	RequestDeferredDiffGeometryRefresh();
}

void SBlueprintHelperReviewPanel::UpdateDetailsSelection()
{
	if (!KismetInspector.IsValid())
	{
		return;
	}

	UObject* DetailsObject = ResolveDetailsObjectForSelectedChange();
	if (DetailsObject)
	{
		KismetInspector->ShowDetailsForSingleObject(
			DetailsObject,
			SKismetInspector::FShowDetailsOptions(GetSelectedTitle(), true));
		return;
	}

	TArray<UObject*> EmptySelection;
	KismetInspector->ShowDetailsForObjects(
		EmptySelection,
		SKismetInspector::FShowDetailsOptions(FText::GetEmpty(), true));
}

EBlueprintHelperReviewSurface SBlueprintHelperReviewPanel::ResolveDetailsSurfaceFromSelectedChange() const
{
	auto ResolveFromChange = [](const FReviewChangeItem& Item)
	{
		if (!Item.IsValid())
		{
			return EBlueprintHelperReviewSurface::Unknown;
		}
		if (FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(*Item))
		{
			return EBlueprintHelperReviewSurface::UMGWidgetTree;
		}
		if (FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(*Item))
		{
			return EBlueprintHelperReviewSurface::DataTable;
		}
		if (FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(*Item))
		{
			return EBlueprintHelperReviewSurface::DataAsset;
		}
		if (FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(*Item))
		{
			return EBlueprintHelperReviewSurface::Details;
		}
		return EBlueprintHelperReviewSurface::Unknown;
	};

	const EBlueprintHelperReviewSurface SelectedSurface = ResolveFromChange(SelectedChange);
	if (SelectedSurface != EBlueprintHelperReviewSurface::Unknown)
	{
		return SelectedSurface;
	}

	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!CurrentAssetPath.IsEmpty() && Item.IsValid() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		const EBlueprintHelperReviewSurface ItemSurface = ResolveFromChange(Item);
		if (ItemSurface != EBlueprintHelperReviewSurface::Unknown)
		{
			return ItemSurface;
		}
	}

	return EBlueprintHelperReviewSurface::Details;
}

bool SBlueprintHelperReviewPanel::ResolveReviewRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor) const
{
	TSharedPtr<SWidget> OverlayWidget;
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Components:
		OverlayWidget = ComponentsDiffStackBox;
		break;
	case EBlueprintHelperReviewSurface::MyBlueprint:
		OverlayWidget = MyBlueprintDiffStackBox;
		break;
	case EBlueprintHelperReviewSurface::Details:
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
	case EBlueprintHelperReviewSurface::DataTable:
	case EBlueprintHelperReviewSurface::DataAsset:
		OverlayWidget = DetailsDiffStackBox;
		break;
	default:
		OutAnchor.Reason = TEXT("unsupported_surface_geometry");
		return false;
	}

	const FString TargetText = FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(Change, Surface);
	if (TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const FString PrimaryAssetPath = Change.AssetPath.IsEmpty() ? ReviewAssetContext.AssetPath : Change.AssetPath;
	if (FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
		PrimaryAssetPath,
		Surface,
		TargetText,
		OverlayWidget,
		OutAnchor))
	{
		return true;
	}

	if (PrimaryAssetPath != ReviewAssetContext.AssetPath)
	{
		return FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
			ReviewAssetContext.AssetPath,
			Surface,
			TargetText,
			OverlayWidget,
			OutAnchor);
	}

	return false;
}

UObject* SBlueprintHelperReviewPanel::ResolveDetailsObjectForSelectedChange() const
{
	const EBlueprintHelperReviewSurface SelectedDetailsSurface = ResolveDetailsSurfaceFromSelectedChange();
	if (SelectedDetailsSurface == EBlueprintHelperReviewSurface::UMGWidgetTree
		|| SelectedDetailsSurface == EBlueprintHelperReviewSurface::DataTable
		|| SelectedDetailsSurface == EBlueprintHelperReviewSurface::DataAsset)
	{
		return nullptr;
	}

	if (SelectedChange.IsValid()
		&& FBlueprintHelperReviewGraphPresenter::ShouldShowChange(*SelectedChange))
	{
		if (UEdGraph* Graph = ResolveGraphForSelectedChange())
		{
			return Graph;
		}
	}

	if (ReviewAssetContext.DefaultObject.IsValid())
	{
		return ReviewAssetContext.DefaultObject.Get();
	}
	return ReviewAssetContext.AssetObject.Get();
}

UEdGraph* SBlueprintHelperReviewPanel::ResolveGraphForSelectedChange() const
{
	return FBlueprintHelperReviewGraphPresenter::ResolveGraphForSelection(
		ReviewAssetContext,
		SelectedChange);
}
