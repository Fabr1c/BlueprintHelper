// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/Review/BlueprintHelperReviewPanelStyle.h"
#include "UI/Review/BlueprintHelperReviewPanelPresenter.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

void SBlueprintHelperReviewPanel::Construct(const FArguments& InArgs)
{
	ReviewPanelPresenter = MakeShared<FBlueprintHelperReviewPanelPresenter>(
		InArgs._ReviewStoreService,
		InArgs._ReviewActionService);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges = InArgs._InitialChanges;
	if (InitialChanges.Num() == 0 && ReviewPanelPresenter.IsValid())
	{
		InitialChanges = ReviewPanelPresenter->LoadPendingVisibleChanges();
	}
	RefreshVisibleChanges(InitialChanges);
	LastVisibleChangeRefreshSignature = BuildVisibleChangeRefreshSignature(InitialChanges);
	if (ReviewPanelPresenter.IsValid())
	{
		PendingReviewChangedHandle = ReviewPanelPresenter->AddPendingReviewChangedHandler(
			FSimpleDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::RefreshFromReviewStoreIfChanged));
	}
	RowGeometryChangedHandle = FBlueprintHelperReviewSlateRowGeometryRegistry::AddRowsChangedHandler(
		FBlueprintHelperReviewSlateRowLifecycleChanged::FDelegate::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::OnRegisteredRowGeometryChanged));
	RowHighlightStateChangedHandle = FBlueprintHelperReviewRowHighlightModel::AddStateChangedHandler(
		FBlueprintHelperReviewRowHighlightStateChanged::FDelegate::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::OnRowHighlightStateChanged));
	if (ChangeItems.Num() > 0)
	{
		SelectedChange = ChangeItems[0];
	}
	LoadReviewAssetFromSelection();
	ConfigureSurfaceViewCoordinator();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
		.BorderBackgroundColor(FBlueprintHelperReviewPanelStyle::GetReviewPanelBackground())
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
	SyncReviewRowHighlightStates(SelectedChange.IsValid() ? SelectedChange->AssetPath : FString());
	RefreshDiffStackWidgets();
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildFinalChangeSidebar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FBlueprintHelperReviewPanelStyle::GetReviewSectionBackground())
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

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildComponentsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FBlueprintHelperReviewPanelStyle::GetReviewSectionBackground())
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::FromString(
						ReviewAssetContext.AssetKind == EBlueprintHelperReviewAssetKind::WidgetBlueprint
							? TEXT("Widget Tree")
							: TEXT("\u7ec4\u4ef6"));
				})
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
						BuildStructurePanelDiffFrames()
					]
				]
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildMyBlueprintPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FBlueprintHelperReviewPanelStyle::GetReviewSectionBackground())
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
					BuildMainWorkspaceWidget()
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(6.0f, 6.0f, 6.0f, 48.0f)
			[
				SAssignNew(MainWorkspaceDiffStackBox, SBox)
				[
					BuildMainWorkspaceDiffFrames()
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
		.BorderBackgroundColor(FBlueprintHelperReviewPanelStyle::GetReviewSectionBackground())
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

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildMainWorkspaceWidget()
{
	const EBlueprintHelperReviewSurface MainSurface =
		FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(ReviewAssetContext.AssetKind);
	if (MainSurface == EBlueprintHelperReviewSurface::DataTable)
	{
		GraphPresenterState.Reset();
		return FBlueprintHelperReviewDataTablePresenter::BuildContent(
			ReviewAssetContext,
			DataTablePresenterState,
			FBlueprintHelperReviewGeometryInvalidated::CreateSP(
				this,
				&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated));
	}
	if (MainSurface == EBlueprintHelperReviewSurface::DataAsset)
	{
		GraphPresenterState.Reset();
		return FBlueprintHelperReviewDataAssetPresenter::BuildContent(
			ReviewAssetContext,
			DataAssetPresenterState,
			FBlueprintHelperReviewGeometryInvalidated::CreateSP(
				this,
				&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated));
	}

	return BuildGraphEditorWidget();
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildGraphEditorWidget()
{
	FBlueprintHelperReviewGraphPresenterArgs Args;
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SelectedChange = SelectedChange;
	const bool bSelectedMatchesGraphNavigation =
		SelectedChange.IsValid()
		&& !RequestedGraphNavigationChangeId.IsEmpty()
		&& SelectedChange->ChangeId == RequestedGraphNavigationChangeId;
	Args.RequestedGraphName = bSelectedMatchesGraphNavigation ? RequestedGraphNavigationGraphName : FString();
	Args.bAllowGraphNavigationWithoutGraphReview =
		bSelectedMatchesGraphNavigation && bAllowGraphNavigationWithoutGraphReview;
	Args.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Args.OnReviewActionIntent = [this](const FBlueprintHelperReviewActionIntent& Intent)
	{
		return OnReviewActionIntent(Intent);
	};
	Args.GetChangeColor = [this](EBlueprintHelperReviewChangeKind Kind)
	{
		return GetChangeColor(Kind);
	};

	return FBlueprintHelperReviewGraphPresenter::BuildContent(Args, GraphPresenterState);
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




