// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "UI/Review/BlueprintHelperReviewPanelSettingsResolver.h"
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
	const FBlueprintHelperReviewPerformanceSettings ReviewPerformanceSettings =
		FBlueprintHelperUiSettingsResolver::LoadReviewPerformanceSettings();
	FBlueprintHelperReviewPerformanceScope Scope(
		TEXT("ReviewPanel.Construct"),
		ReviewPerformanceSettings.TraceWarningMs);
	PendingPageSize = FMath::Max(1, ReviewPerformanceSettings.PendingLoadPageSize);
	PendingScrollPrefetchRows = FMath::Max(0, ReviewPerformanceSettings.PendingLoadScrollPrefetchRows);

	ReviewPanelSettings = FBlueprintHelperReviewPanelSettingsResolver::Load();

	ReviewPanelPresenter = MakeShared<FBlueprintHelperReviewPanelPresenter>(
		InArgs._ReviewStoreService,
		InArgs._ReviewActionService);
	OnValidityCandidatesReady = InArgs._OnValidityCandidatesReady;
	PendingLoadCoordinator = MakeShared<FBlueprintHelperReviewPendingLoadCoordinator>(
		InArgs._ReviewStoreService,
		ReviewPerformanceSettings);

	const auto RatioAt = [](const TArray<float>& Values, const int32 Index, const float DefaultValue)
	{
		return Values.IsValidIndex(Index) ? Values[Index] : DefaultValue;
	};
	const float MainGraphSideRatio = FMath::Max(
		0.0f,
		1.0f
		- RatioAt(ReviewPanelSettings.MainGraphRatio, 0, 0.62f)
		- RatioAt(ReviewPanelSettings.MainGraphRatio, 1, 0.20f));

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges = InArgs._InitialChanges;
	Scope.AddCount(TEXT("initial_changes"), InitialChanges.Num());
	RefreshVisibleChanges(InitialChanges);
	if (InitialChanges.Num() > 0)
	{
		FBlueprintHelperReviewPendingLoadResult InitialPage;
		InitialPage.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
		InitialPage.Changes = InitialChanges;
		InitialPage.TotalMatchingCount = InitialChanges.Num();
		InitialPage.bHasMore = false;
		PagedChangeModel.ApplyPendingLoadResult(InitialPage);
	}
	LastVisibleChangeRefreshSignature = BuildVisibleChangeRefreshSignature(InitialChanges);
	if (ReviewPanelPresenter.IsValid())
	{
		PendingReviewChangedHandle = ReviewPanelPresenter->AddPendingReviewChangedEventHandler(
			FBlueprintHelperReviewStoreChangedMulticast::FDelegate::CreateSP(
				this,
				&SBlueprintHelperReviewPanel::RefreshFromReviewStoreIfChanged));
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
				.Value(RatioAt(ReviewPanelSettings.MainSplitRatio, 0, 0.14f))
				[
					BuildFinalChangeSidebar()
				]
				+ SSplitter::Slot()
				.Value(RatioAt(ReviewPanelSettings.MainSplitRatio, 1, 0.86f))
				[
					SNew(SSplitter)
					.Orientation(Orient_Horizontal)
					+ SSplitter::Slot()
					.Value(MainGraphSideRatio)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(RatioAt(ReviewPanelSettings.ComponentBlueprintSplit, 0, 0.42f))
						.Padding(4.0f)
						[
							BuildComponentsPanel()
						]
						+ SVerticalBox::Slot()
						.FillHeight(RatioAt(ReviewPanelSettings.ComponentBlueprintSplit, 1, 0.58f))
						.Padding(4.0f)
						[
							BuildMyBlueprintPanel()
						]
					]
					+ SSplitter::Slot()
					.Value(RatioAt(ReviewPanelSettings.MainGraphRatio, 0, 0.62f))
					[
						BuildGraphPanel()
					]
					+ SSplitter::Slot()
					.Value(RatioAt(ReviewPanelSettings.MainGraphRatio, 1, 0.20f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(RatioAt(ReviewPanelSettings.RightBottomRatio, 0, 0.76f))
						[
							BuildDetailsPanel()
						]
						+ SVerticalBox::Slot()
						.FillHeight(RatioAt(ReviewPanelSettings.RightBottomRatio, 1, 0.24f))
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
	if (InitialChanges.Num() == 0)
	{
		RequestPendingReviewPage(
			TEXT("construct"),
			EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage);
	}
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
				.OnTreeViewScrolled(this, &SBlueprintHelperReviewPanel::OnChangeTreeScrolled)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SBlueprintHelperReviewPanel::GetPendingPageStatusText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 2.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("加载更多")))
				.Visibility_Lambda([this]()
				{
					return PagedChangeModel.HasMorePages() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.IsEnabled_Lambda([this]()
				{
					return !PagedChangeModel.IsPageRequestInFlight();
				})
				.OnClicked(this, &SBlueprintHelperReviewPanel::OnLoadMorePendingChanges)
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
	if (MainSurface == EBlueprintHelperReviewSurface::Material)
	{
		GraphPresenterState.Reset();
		return FBlueprintHelperReviewMaterialPresenter::BuildContent(
			ReviewAssetContext,
			MaterialPresenterState,
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
	Args.ReviewPanelSettings = ReviewPanelSettings;

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




