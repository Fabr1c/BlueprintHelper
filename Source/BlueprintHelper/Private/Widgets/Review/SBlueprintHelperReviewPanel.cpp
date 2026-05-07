// BlueprintHelper fake Review panel implementation.

#include "Widgets/Review/SBlueprintHelperReviewPanel.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GraphEditor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "SKismetInspector.h"
#include "SMyBlueprint.h"
#include "Services/Review/BlueprintHelperReviewActionService.h"
#include "Services/Review/BlueprintHelperReviewStoreService.h"
#include "SSubobjectBlueprintEditor.h"
#include "Widgets/Review/BlueprintHelperReviewDebugText.h"
#include "Widgets/Review/BlueprintHelperReviewDiffBlockNode.h"
#include "Widgets/Review/BlueprintHelperReviewGraphBounds.h"
#include "Widgets/Review/BlueprintHelperReviewGraphResolver.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const FLinearColor ReviewGreen(0.05f, 0.75f, 0.22f, 0.85f);
	const FLinearColor ReviewRed(0.95f, 0.12f, 0.10f, 0.85f);
	const FLinearColor ReviewYellow(1.0f, 0.72f, 0.08f, 0.85f);
	const FLinearColor ReviewPanelBg(0.015f, 0.015f, 0.015f, 1.0f);
	const FLinearColor ReviewSectionBg(0.035f, 0.035f, 0.035f, 1.0f);
	const FLinearColor ReviewFrameInnerBg(0.06f, 0.06f, 0.06f, 0.92f);

	FText StatusToText(EBlueprintHelperReviewChangeStatus Status)
	{
		return FText::FromString(BlueprintHelperReviewChangeStatusToString(Status));
	}

	bool IsReviewPropertyEditingEnabled()
	{
		return false;
	}

	UBlueprint* LoadReviewBlueprintAsset(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Blueprint;
		}

		FString PackagePath;
		FString AssetName;
		if (AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			&& !AssetName.Contains(TEXT(".")))
		{
			const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
			return LoadObject<UBlueprint>(nullptr, *ObjectPath);
		}

		return nullptr;
	}

	UBlueprint* CreateReviewPreviewBlueprint(const UBlueprint* SourceBlueprint)
	{
		const FName PreviewName = MakeUniqueObjectName(
			GetTransientPackage(),
			UBlueprint::StaticClass(),
			FName(TEXT("BlueprintHelperReviewPreviewBP")));

		UBlueprint* PreviewBlueprint = NewObject<UBlueprint>(
			GetTransientPackage(),
			PreviewName,
			RF_Transient);

		if (SourceBlueprint)
		{
			PreviewBlueprint->BlueprintType = SourceBlueprint->BlueprintType;
			PreviewBlueprint->ParentClass = SourceBlueprint->ParentClass;
			PreviewBlueprint->GeneratedClass = SourceBlueprint->GeneratedClass;
			PreviewBlueprint->SkeletonGeneratedClass = SourceBlueprint->SkeletonGeneratedClass;
		}

		return PreviewBlueprint;
	}

	void AttachPreviewGraphToMatchingBlueprintList(
		const UBlueprint* SourceBlueprint,
		const UEdGraph* SourceGraph,
		UBlueprint* PreviewBlueprint,
		UEdGraph* PreviewGraph)
	{
		if (!SourceBlueprint || !SourceGraph || !PreviewBlueprint || !PreviewGraph)
		{
			return;
		}

		if (SourceBlueprint->UbergraphPages.Contains(SourceGraph))
		{
			PreviewBlueprint->UbergraphPages.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->FunctionGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->FunctionGraphs.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->MacroGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->MacroGraphs.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->DelegateSignatureGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->DelegateSignatureGraphs.Add(PreviewGraph);
		}
	}

	class SBlueprintHelperReviewDiffFrame : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlueprintHelperReviewDiffFrame)
			: _FrameColor(FSlateColor(FLinearColor::Transparent))
			, _ShowActions(false)
			, _FillBackground(true)
		{
		}

			SLATE_ATTRIBUTE(FSlateColor, FrameColor)
			SLATE_ATTRIBUTE(bool, ShowActions)
			SLATE_ARGUMENT(bool, FillBackground)
			SLATE_EVENT(FOnClicked, OnAccept)
			SLATE_EVENT(FOnClicked, OnReject)
			SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			FrameColor = InArgs._FrameColor;
			ShowActions = InArgs._ShowActions;
			bFillBackground = InArgs._FillBackground;
			OnAccept = InArgs._OnAccept;
			OnReject = InArgs._OnReject;

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetFrameBrush)
					.Padding(3.0f)
					[
						SNew(SBorder)
						.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetInnerBrush)
						.Padding(0.0f)
						[
							InArgs._Content.Widget
						]
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(0.0f, 0.0f, 8.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(&ActionsBrush)
					.Padding(5.0f)
					.Visibility(this, &SBlueprintHelperReviewDiffFrame::GetActionsVisibility)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Accept")))
							.OnClicked(OnAccept)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Reject")))
							.OnClicked(OnReject)
						]
					]
				]
			];
		}

	private:
		FSlateColor GetFrameColor() const
		{
			return FrameColor.Get();
		}

		const FSlateBrush* GetFrameBrush() const
		{
			FrameBrush = FSlateRoundedBoxBrush(
				FLinearColor::Transparent,
				7.0f,
				GetFrameColor().GetSpecifiedColor(),
				4.0f);
			return &FrameBrush;
		}

		const FSlateBrush* GetInnerBrush() const
		{
			InnerBrush = FSlateRoundedBoxBrush(
				bFillBackground ? ReviewFrameInnerBg : FLinearColor::Transparent,
				5.0f);
			return &InnerBrush;
		}

		EVisibility GetActionsVisibility() const
		{
			return ShowActions.Get(false) && IsHovered()
				? EVisibility::Visible
				: EVisibility::Collapsed;
		}

		TAttribute<FSlateColor> FrameColor;
		TAttribute<bool> ShowActions;
		FOnClicked OnAccept;
		FOnClicked OnReject;
		bool bFillBackground = true;
		mutable FSlateRoundedBoxBrush FrameBrush = FSlateRoundedBoxBrush(
			FLinearColor::Transparent,
			7.0f,
			FLinearColor::Transparent,
			4.0f);
		mutable FSlateRoundedBoxBrush InnerBrush = FSlateRoundedBoxBrush(ReviewFrameInnerBg, 5.0f);
		FSlateRoundedBoxBrush ActionsBrush = FSlateRoundedBoxBrush(
			FLinearColor(0.02f, 0.02f, 0.02f, 0.95f),
			5.0f);
	};
}

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
	LoadReviewBlueprintFromSelection();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
		.BorderBackgroundColor(ReviewPanelBg)
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
	LoadReviewBlueprintFromSelection();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildGraphEditorWidget());
	}
	UpdateDetailsSelection();
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
	return BlueprintHelperReviewDebugText::BuildCopyableText(DebugMessages);
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
		.BorderBackgroundColor(ReviewSectionBg)
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
		.BorderBackgroundColor(ReviewSectionBg)
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
							&BlueprintHelperReviewShouldShowInComponents,
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
		.BorderBackgroundColor(ReviewSectionBg)
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
							&BlueprintHelperReviewShouldShowInMyBlueprint,
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
		.BorderBackgroundColor(ReviewSectionBg)
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
					BuildReadonlyDetailsWidget()
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(6.0f, 34.0f, 6.0f, 0.0f)
				[
					SAssignNew(DetailsDiffStackBox, SBox)
					[
						BuildPanelDiffFrames(
							&BlueprintHelperReviewShouldShowInDetails,
							EBlueprintHelperReviewSurface::Details)
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
		.BorderBackgroundColor(ReviewSectionBg)
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
	UEdGraph* SourceGraph = ResolveGraphForSelectedChange();
	PreviewBlueprint.Reset();
	PreviewGraph.Reset();

	const UBlueprint* SourceBlueprint = ReviewBlueprint.Get();
	if (!SourceGraph && SelectedChange.IsValid() && !SelectedChange->GraphName.IsEmpty())
	{
		AddDebugMessage(FString::Printf(
			TEXT("GraphEditor source graph missing selectedGraph=\"%s\" asset=\"%s\"; using empty Review graph."),
			*SelectedChange->GraphName,
			*SelectedChange->AssetPath));
	}

	PreviewBlueprint = TStrongObjectPtr<UBlueprint>(CreateReviewPreviewBlueprint(SourceBlueprint));

	if (SourceGraph && PreviewBlueprint.IsValid())
	{
		PreviewGraph = TStrongObjectPtr<UEdGraph>(FEdGraphUtilities::CloneGraph(SourceGraph, PreviewBlueprint.Get()));
		if (PreviewGraph.IsValid())
		{
			PreviewGraph->SetFlags(RF_Transient);
			PreviewGraph->bEditable = false;
			AttachPreviewGraphToMatchingBlueprintList(
				SourceBlueprint,
				SourceGraph,
				PreviewBlueprint.Get(),
				PreviewGraph.Get());
		}
	}

	if (!PreviewGraph.IsValid())
	{
		if (!PreviewBlueprint.IsValid())
		{
			PreviewBlueprint = TStrongObjectPtr<UBlueprint>(CreateReviewPreviewBlueprint(SourceBlueprint));
		}
		PreviewGraph = TStrongObjectPtr<UEdGraph>(NewObject<UEdGraph>(
			PreviewBlueprint.Get(),
			NAME_None,
			RF_Transient));
		PreviewGraph->Schema = UEdGraphSchema_K2::StaticClass();
		PreviewGraph->bEditable = false;
	}

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = FText::FromString(TEXT("Review"));
	Appearance.InstructionText = FText::FromString(TEXT("Read-only Review Graph"));
	Appearance.ReadOnlyText = FText::FromString(TEXT("Review Only"));

	TSharedRef<SGraphEditor> Editor = SAssignNew(GraphEditorWidget, SGraphEditor)
		.IsEditable(false)
		.DisplayAsReadOnly(false)
		.GraphToEdit(PreviewGraph.Get())
		.Appearance(Appearance)
		.ShowGraphStateOverlay(false);

	AddDebugMessage(FString::Printf(
		TEXT("GraphEditor build sourceGraph=\"%s\" previewGraph=\"%s\" previewNodes=%d timer=disabled"),
		SourceGraph ? *SourceGraph->GetName() : TEXT("<none>"),
		PreviewGraph.IsValid() ? *PreviewGraph->GetName() : TEXT("<none>"),
		PreviewGraph.IsValid() ? PreviewGraph->Nodes.Num() : 0));
	AddGraphDiffBlocks(PreviewGraph.Get(), SourceGraph, Editor);
	Editor->NotifyGraphChanged();

	JumpToSelectedGraphDiffBlock();

	return Editor;
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

void SBlueprintHelperReviewPanel::AddGraphDiffBlocks(
	UEdGraph* PreviewGraphToEdit,
	const UEdGraph* SourceGraph,
	const TSharedPtr<SGraphEditor>& GraphEditorForBounds)
{
	if (!PreviewGraphToEdit)
	{
		return;
	}

	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	const FString GraphName = SourceGraph ? SourceGraph->GetName() : (SelectedChange.IsValid() ? SelectedChange->GraphName : FString());
	const UEdGraph* BoundsGraph = SourceGraph ? SourceGraph : PreviewGraphToEdit;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || !BlueprintHelperReviewShouldShowInGraph(*Item))
		{
			continue;
		}
		if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D(760.0f, 180.0f);
		FString BoundsDebug;
		if (!BuildGraphBoundsForChange(Item, BoundsGraph, GraphName, GraphEditorForBounds, Position, Size, &BoundsDebug))
		{
			AddDebugMessage(FString::Printf(
				TEXT("GraphDiff bounds failed change=%s selected=%d boundsGraph=\"%s\" %s"),
				*Item->ChangeId,
				Item == SelectedChange ? 1 : 0,
				BoundsGraph ? *BoundsGraph->GetName() : TEXT("<none>"),
				*BoundsDebug));
			if (Item != SelectedChange)
			{
				continue;
			}
			Position = FVector2D(-380.0f, -90.0f);
			AddDebugMessage(FString::Printf(
				TEXT("GraphDiff fallback rect applied change=%s pos=(%.1f,%.1f) size=(%.1f,%.1f)"),
				*Item->ChangeId,
				static_cast<double>(Position.X),
				static_cast<double>(Position.Y),
				static_cast<double>(Size.X),
				static_cast<double>(Size.Y)));
		}
		else
		{
			AddDebugMessage(FString::Printf(
				TEXT("GraphDiff bounds change=%s selected=%d boundsGraph=\"%s\" %s"),
				*Item->ChangeId,
				Item == SelectedChange ? 1 : 0,
				BoundsGraph ? *BoundsGraph->GetName() : TEXT("<none>"),
				*BoundsDebug));
		}

		UBlueprintHelperReviewDiffBlockNode* DiffNode = NewObject<UBlueprintHelperReviewDiffBlockNode>(PreviewGraphToEdit);
		DiffNode->SetFlags(RF_Transient);
		DiffNode->CreateNewGuid();
		DiffNode->NodePosX = FMath::FloorToInt(Position.X);
		DiffNode->NodePosY = FMath::FloorToInt(Position.Y);
		DiffNode->NodeWidth = FMath::CeilToInt(Size.X);
		DiffNode->NodeHeight = FMath::CeilToInt(Size.Y);
		DiffNode->Configure(
			Item->ChangeId,
			Item->DisplayLabel,
			GetChangeColor(Item->ChangeKind).GetSpecifiedColor(),
			Item == SelectedChange,
			[this](const FString& ChangeId)
			{
				return OnAcceptChangeId(ChangeId);
			},
			[this](const FString& ChangeId)
			{
				return OnRejectChangeId(ChangeId);
			});
		PreviewGraphToEdit->AddNode(DiffNode, false, false);
		AddDebugMessage(FString::Printf(
			TEXT("GraphDiff node created change=%s graph=\"%s\" pos=(%d,%d) size=(%d,%d) previewNodes=%d"),
			*Item->ChangeId,
			*GraphName,
			DiffNode->NodePosX,
			DiffNode->NodePosY,
			DiffNode->NodeWidth,
			DiffNode->NodeHeight,
			PreviewGraphToEdit->Nodes.Num()));
	}
}

void SBlueprintHelperReviewPanel::JumpToSelectedGraphDiffBlock()
{
	if (!SelectedChange.IsValid() || !PreviewGraph.IsValid() || !GraphEditorWidget.IsValid())
	{
		AddDebugMessage(TEXT("GraphDiff jump skipped because selection, preview graph, or editor is invalid."));
		return;
	}

	bool bJumped = false;
	for (UEdGraphNode* Node : PreviewGraph->Nodes)
	{
		if (const UBlueprintHelperReviewDiffBlockNode* DiffNode = Cast<UBlueprintHelperReviewDiffBlockNode>(Node))
		{
			if (DiffNode->ChangeId == SelectedChange->ChangeId)
			{
				GraphEditorWidget->JumpToNode(Node, false, false);
				bJumped = true;
				AddDebugMessage(FString::Printf(
					TEXT("GraphDiff jump change=%s pos=(%d,%d) size=(%d,%d)"),
					*SelectedChange->ChangeId,
					DiffNode->NodePosX,
					DiffNode->NodePosY,
					DiffNode->NodeWidth,
					DiffNode->NodeHeight));
				break;
			}
		}
	}

	if (!bJumped)
	{
		AddDebugMessage(FString::Printf(
			TEXT("GraphDiff jump missed change=%s previewNodes=%d"),
			*SelectedChange->ChangeId,
			PreviewGraph->Nodes.Num()));
	}
}

bool SBlueprintHelperReviewPanel::BuildGraphBoundsForChange(
	const FReviewChangeItem& Item,
	const UEdGraph* PreviewGraphToEdit,
	const FString& GraphName,
	const TSharedPtr<SGraphEditor>& GraphEditorForBounds,
	FVector2D& OutPosition,
	FVector2D& OutSize,
	FString* OutDebugSummary) const
{
	if (!Item.IsValid())
	{
		if (OutDebugSummary)
		{
			*OutDebugSummary = TEXT("built=0 invalid change item");
		}
		return false;
	}

	if (Item->AtomicTargets.Num() > 0)
	{
		return BlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
			Item->AtomicTargets,
			PreviewGraphToEdit,
			GraphName,
			GraphEditorForBounds,
			OutPosition,
			OutSize,
			OutDebugSummary);
	}

	if (!Item->GraphName.IsEmpty() && !GraphName.IsEmpty() && Item->GraphName != GraphName)
	{
		if (OutDebugSummary)
		{
			*OutDebugSummary = FString::Printf(
				TEXT("built=0 graph mismatch itemGraph=\"%s\" currentGraph=\"%s\""),
				*Item->GraphName,
				*GraphName);
		}
		return false;
	}

	FBlueprintHelperReviewAtomicTarget FallbackTarget;
	FallbackTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	FallbackTarget.GraphName = Item->GraphName;
	FallbackTarget.TargetKey = Item->LocationKey;
	FallbackTarget.NodeGuid = Item->LocationKey;
	TArray<FBlueprintHelperReviewAtomicTarget> FallbackTargets;
	FallbackTargets.Add(FallbackTarget);
	return BlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		FallbackTargets,
		PreviewGraphToEdit,
		GraphName,
		GraphEditorForBounds,
		OutPosition,
		OutSize,
		OutDebugSummary);
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
	if (UBlueprint* Blueprint = ReviewBlueprint.Get())
	{
		if (Blueprint->GeneratedClass)
		{
			if (AActor* ActorCDO = Blueprint->GeneratedClass->GetDefaultObject<AActor>())
			{
				return SNew(SSubobjectBlueprintEditor)
					.ObjectContext(ActorCDO)
					.AllowEditing(false)
					.HideComponentClassCombo(true);
			}
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
			.Text(FText::FromString(TEXT("No Blueprint component tree loaded.")))
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyMyBlueprintWidget()
{
	if (UBlueprint* Blueprint = ReviewBlueprint.Get())
	{
		TSharedRef<SMyBlueprint> Widget = SAssignNew(MyBlueprintWidget, SMyBlueprint, TWeakPtr<FBlueprintEditor>(), Blueprint);
		if (KismetInspector.IsValid())
		{
			MyBlueprintWidget->SetInspector(KismetInspector);
		}
		return Widget;
	}

	MyBlueprintWidget.Reset();
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
			.Text(FText::FromString(TEXT("No Blueprint outline loaded.")))
		];
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyDetailsWidget()
{
	TSharedRef<SKismetInspector> Inspector = SAssignNew(KismetInspector, SKismetInspector)
		.HideNameArea(true)
		.ViewIdentifier(FName(TEXT("BlueprintHelperReviewInspector")))
		.MyBlueprintWidget(MyBlueprintWidget)
		.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic(&IsReviewPropertyEditingEnabled))
		.ShowLocalVariables(true);
	if (MyBlueprintWidget.IsValid())
	{
		MyBlueprintWidget->SetInspector(KismetInspector);
	}
	return Inspector;
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
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	TMap<FString, int32> BucketRowCounts;
	int32 VisibleFrameCount = 0;

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

		const FString Bucket = GetPanelFrameBucket(Item, Surface);
		int32& BucketRowIndex = BucketRowCounts.FindOrAdd(Bucket);

		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		if (!TryBuildPanelFrameGeometry(Item, Surface, Bucket, BucketRowIndex, Position, Size))
		{
			++BucketRowIndex;
			continue;
		}

		Canvas->AddSlot()
		.Position(Position)
		.Size(Size)
		[
			BuildPanelDiffFrame(Item)
		];

		++BucketRowIndex;
		++VisibleFrameCount;
	}

	if (VisibleFrameCount == 0)
	{
		return SNullWidget::NullWidget;
	}

	return Canvas;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildPanelDiffFrame(FReviewChangeItem Item)
{
	const FSlateColor FrameColor = Item == SelectedChange && Item.IsValid()
		? GetSelectedDiffColor()
		: (Item.IsValid() ? GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent));

	return SNew(SBlueprintHelperReviewDiffFrame)
		.FrameColor(FrameColor)
		.ShowActions(Item.IsValid())
		.FillBackground(false)
		.OnAccept(this, &SBlueprintHelperReviewPanel::OnAcceptChange, Item)
		.OnReject(this, &SBlueprintHelperReviewPanel::OnRejectChange, Item)
		[
			SNullWidget::NullWidget
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

FString SBlueprintHelperReviewPanel::BuildPanelTargetSearchText(
	FReviewChangeItem Item,
	EBlueprintHelperReviewSurface Surface) const
{
	if (!Item.IsValid())
	{
		return FString();
	}

	FString Text;
	auto AppendToken = [&Text](const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Text += TEXT(" ");
			Text += Value;
		}
	};

	AppendToken(Item->LocationKey);
	AppendToken(Item->DisplayLabel);
	AppendToken(Item->BeforeSummary);
	AppendToken(Item->AfterSummary);

	for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
	{
		if (Target.Surface != Surface)
		{
			continue;
		}

		AppendToken(Target.TargetKey);
		AppendToken(Target.VisualGroupKey);
		AppendToken(Target.DisplayLabel);
		AppendToken(Target.ComponentPath);
		AppendToken(Target.PropertyPath);
		AppendToken(Target.PinPath);
	}

	Text.ToLowerInline();
	return Text;
}

FString SBlueprintHelperReviewPanel::GetPanelFrameBucket(
	FReviewChangeItem Item,
	EBlueprintHelperReviewSurface Surface) const
{
	const FString Text = BuildPanelTargetSearchText(Item, Surface);

	if (Surface == EBlueprintHelperReviewSurface::Components)
	{
		return TEXT("component");
	}

	if (Surface == EBlueprintHelperReviewSurface::Details)
	{
		return TEXT("details");
	}

	if (Text.Contains(TEXT("dispatcher")) || Text.Contains(TEXT("delegate")))
	{
		return TEXT("dispatcher");
	}
	if (Text.Contains(TEXT("variable"))
		|| Text.Contains(TEXT("property"))
		|| Text.Contains(TEXT("smokevalue"))
		|| Text.Contains(TEXT("fakediffproperty")))
	{
		return TEXT("variable");
	}
	if (Text.Contains(TEXT("component")))
	{
		return TEXT("component_variable");
	}
	if (Text.Contains(TEXT("macro")))
	{
		return TEXT("macro");
	}
	return TEXT("function");
}

bool SBlueprintHelperReviewPanel::TryBuildPanelFrameGeometry(
	FReviewChangeItem Item,
	EBlueprintHelperReviewSurface Surface,
	const FString& Bucket,
	int32 BucketRowIndex,
	FVector2D& OutPosition,
	FVector2D& OutSize) const
{
	if (!Item.IsValid())
	{
		return false;
	}

	const FString Text = BuildPanelTargetSearchText(Item, Surface);
	if (Surface == EBlueprintHelperReviewSurface::Components)
	{
		const float RowTop = Text.Contains(TEXT("defaultsceneroot")) ? 66.0f : 92.0f + BucketRowIndex * 24.0f;
		const float RowLeft = Text.Contains(TEXT("defaultsceneroot")) ? 30.0f : 44.0f;
		OutPosition = FVector2D(RowLeft, RowTop);
		OutSize = FVector2D(250.0f, 26.0f);
		return true;
	}

	if (Surface == EBlueprintHelperReviewSurface::Details)
	{
		OutPosition = FVector2D(6.0f, 42.0f + BucketRowIndex * 30.0f);
		OutSize = FVector2D(320.0f, 28.0f);
		return true;
	}

	if (Surface != EBlueprintHelperReviewSurface::MyBlueprint)
	{
		return false;
	}

	if (Bucket == TEXT("function"))
	{
		float RowTop = 158.0f + BucketRowIndex * 30.0f;
		if (Text.Contains(TEXT("fakedifffunction")))
		{
			RowTop = 218.0f;
		}
		else if (Text.Contains(TEXT("bh_smoke")))
		{
			RowTop = 188.0f;
		}
		else if (Text.Contains(TEXT("construction")))
		{
			RowTop = 158.0f;
		}

		OutPosition = FVector2D(6.0f, RowTop);
		OutSize = FVector2D(360.0f, 28.0f);
		return true;
	}

	if (Bucket == TEXT("component_variable"))
	{
		const float RowTop = Text.Contains(TEXT("defaultsceneroot")) ? 354.0f : 326.0f + BucketRowIndex * 24.0f;
		OutPosition = FVector2D(28.0f, RowTop);
		OutSize = FVector2D(245.0f, 24.0f);
		return true;
	}

	if (Bucket == TEXT("variable"))
	{
		float RowTop = 405.0f + BucketRowIndex * 31.0f;
		if (Text.Contains(TEXT("fakediffproperty")))
		{
			RowTop = 436.0f;
		}
		else if (Text.Contains(TEXT("smokevalue")))
		{
			RowTop = 405.0f;
		}

		OutPosition = FVector2D(6.0f, RowTop);
		OutSize = FVector2D(360.0f, 28.0f);
		return true;
	}

	if (Bucket == TEXT("dispatcher"))
	{
		OutPosition = FVector2D(6.0f, 488.0f + BucketRowIndex * 30.0f);
		OutSize = FVector2D(360.0f, 28.0f);
		return true;
	}

	if (Bucket == TEXT("macro"))
	{
		OutPosition = FVector2D(6.0f, 280.0f + BucketRowIndex * 30.0f);
		OutSize = FVector2D(360.0f, 28.0f);
		return true;
	}

	return false;
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
	return SNew(SBlueprintHelperReviewDiffFrame)
		.FrameColor(Item.IsValid() ? GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent))
		.ShowActions(bShowActions && Item.IsValid())
		.OnAccept(this, &SBlueprintHelperReviewPanel::OnAcceptChange, Item)
		.OnReject(this, &SBlueprintHelperReviewPanel::OnRejectChange, Item)
		[
			Content
		];
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
		LoadReviewBlueprintFromSelection();
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

	ChangeItems.RemoveAll([&AssetPath](const FReviewChangeItem& Item)
	{
		return Item.IsValid() && (AssetPath.IsEmpty() || Item->AssetPath == AssetPath);
	});

	SelectedChange = ChangeItems.Num() > 0 ? ChangeItems[0] : FReviewChangeItem();
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	RefreshDiffStackWidgets();
	LoadReviewBlueprintFromSelection();
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
			Item->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Item->NeedsActionReason = TEXT("Reject requires archive-baseline rollback service.");
		}
	}

	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	RefreshDiffStackWidgets();
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
		? FText::Format(FText::FromString(TEXT("Status: {0}")), StatusToText(SelectedChange->Status))
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
		return FSlateColor(ReviewGreen);
	}
	if (ColorName == TEXT("red"))
	{
		return FSlateColor(ReviewRed);
	}
	if (ColorName == TEXT("yellow"))
	{
		return FSlateColor(ReviewYellow);
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

void SBlueprintHelperReviewPanel::RefreshDiffStackWidgets()
{
	if (ComponentsDiffStackBox.IsValid())
	{
		ComponentsDiffStackBox->SetContent(BuildPanelDiffFrames(
			&BlueprintHelperReviewShouldShowInComponents,
			EBlueprintHelperReviewSurface::Components));
	}
	if (MyBlueprintDiffStackBox.IsValid())
	{
		MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
			&BlueprintHelperReviewShouldShowInMyBlueprint,
			EBlueprintHelperReviewSurface::MyBlueprint));
	}
	if (DetailsDiffStackBox.IsValid())
	{
		DetailsDiffStackBox->SetContent(BuildPanelDiffFrames(
			&BlueprintHelperReviewShouldShowInDetails,
			EBlueprintHelperReviewSurface::Details));
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SBlueprintHelperReviewPanel::LoadReviewBlueprintFromSelection()
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

	UBlueprint* LoadedBlueprint = LoadReviewBlueprintAsset(AssetPath);
	if (LoadedBlueprint == ReviewBlueprint.Get())
	{
		return;
	}

	ReviewBlueprint = LoadedBlueprint;
	if (ComponentsContentBox.IsValid())
	{
		ComponentsContentBox->SetContent(BuildReadonlyComponentsWidget());
	}
	if (MyBlueprintContentBox.IsValid())
	{
		MyBlueprintContentBox->SetContent(BuildReadonlyMyBlueprintWidget());
	}
	if (KismetInspector.IsValid())
	{
		if (MyBlueprintWidget.IsValid())
		{
			MyBlueprintWidget->SetInspector(KismetInspector);
		}
	}
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

UObject* SBlueprintHelperReviewPanel::ResolveDetailsObjectForSelectedChange() const
{
	if (SelectedChange.IsValid() && BlueprintHelperReviewShouldShowInGraph(*SelectedChange))
	{
		if (UEdGraph* Graph = ResolveGraphForSelectedChange())
		{
			return Graph;
		}
	}

	return ReviewBlueprint.Get();
}

UEdGraph* SBlueprintHelperReviewPanel::ResolveGraphForSelectedChange() const
{
	const UBlueprint* Blueprint = ReviewBlueprint.Get();
	const FString RequestedGraphName = SelectedChange.IsValid() ? SelectedChange->GraphName : FString();
	return BlueprintHelperReviewGraphResolver::ResolveGraphForReviewSelection(Blueprint, RequestedGraphName);
}
