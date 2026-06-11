// BlueprintHelper TaskSpec / ReadContext workbench Slate widget.

#include "UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h"

#include "Entry/BlueprintHelper.h"
#include "Styling/AppStyle.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchPresenter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"


class FBlueprintHelperTaskSpecWorkbenchLocalSettings
{
public:
	static TMap<const SBlueprintHelperTaskSpecWorkbench*, FBlueprintHelperTaskSpecWorkbenchSettings>& SettingsByWidget()
	{
		static TMap<const SBlueprintHelperTaskSpecWorkbench*, FBlueprintHelperTaskSpecWorkbenchSettings> Settings;
		return Settings;
	}

	static const FBlueprintHelperTaskSpecWorkbenchSettings& GetSettings(const SBlueprintHelperTaskSpecWorkbench* Widget)
	{
		if (const FBlueprintHelperTaskSpecWorkbenchSettings* Settings = SettingsByWidget().Find(Widget))
		{
			return *Settings;
		}

		static const FBlueprintHelperTaskSpecWorkbenchSettings DefaultSettings;
		return DefaultSettings;
	}
};

void SBlueprintHelperTaskSpecWorkbench::Construct(const FArguments& InArgs)
{
	GraphResolver = InArgs._GraphResolver;
	FBlueprintHelperTaskSpecWorkbenchLocalSettings::SettingsByWidget().Add(
		this,
		FBlueprintHelperUiSettingsResolver::LoadTaskSpecWorkbenchSettings());
	const FBlueprintHelperTaskSpecWorkbenchSettings& WorkbenchSettings =
		FBlueprintHelperTaskSpecWorkbenchLocalSettings::GetSettings(this);
	Presenter = MakeShared<FBlueprintHelperTaskSpecWorkbenchPresenter>([this]()
	{
		return GetCurrentTargetGraph();
	});
	CurrentSnapshot = Presenter->GetSnapshot();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(WorkbenchSettings.TopPadding)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(WorkbenchSettings.ButtonSpacing)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Export logicflow")))
				.ToolTipText(FText::FromString(TEXT("Recommended first: exports compact LogicFlow.v1 execution/data flow for quickly understanding simple entries. Not an anchor source.")))
				.OnClicked(this, &SBlueprintHelperTaskSpecWorkbench::OnExportLogicFlowClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(WorkbenchSettings.ButtonSpacing)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Export logicjson")))
				.ToolTipText(FText::FromString(TEXT("Exports structured LogicJson with the most detail. Recommended for precise analysis, diff, patch/merge anchors, and debug.")))
				.OnClicked(this, &SBlueprintHelperTaskSpecWorkbench::OnExportLogicJsonClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(FText::FromString(CurrentSnapshot.StatusText))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(WorkbenchSettings.MainSplitRatio.X)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("CallFunction")))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(CandidateListView, SListView<TSharedPtr<FBlueprintHelperCallFunctionCardModel>>)
					.ListItemsSource(&CandidateCardItems)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow(this, &SBlueprintHelperTaskSpecWorkbench::GenerateCandidateCardRow)
				]
			]
			+ SSplitter::Slot()
			.Value(WorkbenchSettings.MainSplitRatio.Y)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(WorkbenchSettings.LeftSplitRatio.X)
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(MainTextBox, SMultiLineEditableTextBox)
					.HintText(FText::FromString(TEXT("Paste BlueprintHelper.TaskSpec.v1 JSON or Blueprint function T3D text here.")))
					.OnTextChanged(this, &SBlueprintHelperTaskSpecWorkbench::OnInputTextChanged)
				]
				+ SVerticalBox::Slot()
				.FillHeight(WorkbenchSettings.LeftSplitRatio.Y)
				[
					SAssignNew(PreviewContainer, SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
					.Padding(WorkbenchSettings.PreviewContainerPadding)
					[
						BuildPreviewContent()
					]
				]
			]
		]
	];

	Presenter->SetEventSink([this](const FBlueprintHelperTaskSpecWorkbenchPresenterEvent& Event)
	{
		HandlePresenterEvent(Event);
	});
	RefreshFromSnapshot(Presenter->GetSnapshot());
}

FReply SBlueprintHelperTaskSpecWorkbench::OnExportLogicFlowClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperTaskSpecWorkbenchVisualEvent::ExportReadContext(
				EBlueprintHelperReadContextExportFormat::LogicFlow))
		: FReply::Handled();
}

FReply SBlueprintHelperTaskSpecWorkbench::OnExportLogicJsonClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperTaskSpecWorkbenchVisualEvent::ExportReadContext(
				EBlueprintHelperReadContextExportFormat::LogicJson))
		: FReply::Handled();
}

void SBlueprintHelperTaskSpecWorkbench::OnInputTextChanged(const FText& InText)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperTaskSpecWorkbenchVisualEvent::InputTextChanged(InText.ToString()));
	}
}

void SBlueprintHelperTaskSpecWorkbench::HandlePresenterEvent(
	const FBlueprintHelperTaskSpecWorkbenchPresenterEvent& Event)
{
	if (Event.bRefreshView)
	{
		RefreshFromSnapshot(Event.Snapshot);
	}
}

void SBlueprintHelperTaskSpecWorkbench::RefreshFromSnapshot(
	const FBlueprintHelperTaskSpecWorkbenchSnapshot& Snapshot)
{
	CurrentSnapshot = Snapshot;
	CandidateCardItems.Reset();
	for (const FBlueprintHelperCallFunctionCardModel& Card : CurrentSnapshot.CandidateCards)
	{
		CandidateCardItems.Add(MakeShared<FBlueprintHelperCallFunctionCardModel>(Card));
	}

	if (CandidateListView.IsValid())
	{
		CandidateListView->RequestListRefresh();
	}
	if (PreviewContainer.IsValid())
	{
		PreviewContainer->SetContent(BuildPreviewContent());
	}
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(CurrentSnapshot.StatusText));
	}
}

TSharedRef<ITableRow> SBlueprintHelperTaskSpecWorkbench::GenerateCandidateCardRow(
	TSharedPtr<FBlueprintHelperCallFunctionCardModel> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SVerticalBox> CardBox = SNew(SVerticalBox);
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FBlueprintHelperCallFunctionCardModel>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Invalid CallFunction card")))
		];
	}

	CardBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Item->Query.IsEmpty() ? TEXT("(empty call)") : Item->Query))
	];

	CardBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Item->StatusText))
		.AutoWrapText(true)
	];

	if (Item->Candidates.Num() == 0)
	{
		CardBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No candidates")))
		];
	}

	for (const FBlueprintHelperCallFunctionCandidateRowModel& Candidate : Item->Candidates)
	{
		const FString CardId = Item->CardId;
		const FString CandidateId = Candidate.CandidateId;
		const FString PrimaryText = Candidate.DisplayName.IsEmpty()
			? Candidate.NativeFunctionName
			: Candidate.DisplayName;
		const FString SecondaryText = FString::Printf(
			TEXT("%s | score=%d"),
			*Candidate.Category,
			Candidate.Score);

		CardBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(PrimaryText))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(SecondaryText))
						.AutoWrapText(true)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>(TEXT("RadioButton")))
					.IsChecked_Lambda([this, CardId, CandidateId]()
					{
						return IsCandidateSelected(CardId, CandidateId)
							? ECheckBoxState::Checked
							: ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, CardId, CandidateId](ECheckBoxState NewState)
					{
						OnCandidateCheckStateChanged(NewState, CardId, CandidateId);
					})
				]
			]
		];
	}

	return SNew(STableRow<TSharedPtr<FBlueprintHelperCallFunctionCardModel>>, OwnerTable)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(6.0f)
		[
			CardBox
		]
	];
}

bool SBlueprintHelperTaskSpecWorkbench::IsCandidateSelected(
	const FString& CardId,
	const FString& CandidateId) const
{
	return CurrentSnapshot.SelectedCardId == CardId
		&& CurrentSnapshot.SelectedCandidateId == CandidateId;
}

void SBlueprintHelperTaskSpecWorkbench::OnCandidateCheckStateChanged(
	ECheckBoxState NewState,
	FString CardId,
	FString CandidateId)
{
	if (NewState == ECheckBoxState::Checked && Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperTaskSpecWorkbenchVisualEvent::CandidateSelected(CardId, CandidateId));
	}
}

TSharedRef<SWidget> SBlueprintHelperTaskSpecWorkbench::BuildPreviewContent() const
{
		const FBlueprintHelperTaskSpecWorkbenchSettings& WorkbenchSettings =
			FBlueprintHelperTaskSpecWorkbenchLocalSettings::GetSettings(this);
TSharedRef<SVerticalBox> NonGraphBox = SNew(SVerticalBox);
	TSharedRef<SGridPanel> GraphGrid = SNew(SGridPanel);
	int32 NonGraphCount = 0;
	int32 GraphCount = 0;

	for (const FBlueprintHelperTaskSpecPreviewBlock& Block : CurrentSnapshot.Preview.Blocks)
	{
		if (Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic)
		{
			GraphGrid->AddSlot(Block.Column, Block.Row)
			.Padding(4.0f)
			[
				BuildPreviewBlockWidget(Block)
			];
			++GraphCount;
		}
		else
		{
			NonGraphBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BuildPreviewBlockWidget(Block)
			];
			++NonGraphCount;
		}
	}

	if (NonGraphCount == 0)
	{
		NonGraphBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("No non-graph blocks")))
		];
	}
	if (GraphCount == 0)
	{
		GraphGrid->AddSlot(0, 0)
		.Padding(4.0f)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("No graph logic blocks")))
		];
	}

	return SNew(SScrollBox)
	.Orientation(Orient_Horizontal)
	+ SScrollBox::Slot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(260.0f)
			[
				NonGraphBox
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			GraphGrid
		]
	];
}

TSharedRef<SWidget> SBlueprintHelperTaskSpecWorkbench::BuildPreviewBlockWidget(
	const FBlueprintHelperTaskSpecPreviewBlock& Block) const
{
	const FBlueprintHelperTaskSpecWorkbenchSettings& WorkbenchSettings =
		FBlueprintHelperTaskSpecWorkbenchLocalSettings::GetSettings(this);
	FLinearColor BlockColor = WorkbenchSettings.DefaultBlockColor;
	if (Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic)
	{
		BlockColor = WorkbenchSettings.GraphLogicBlockColor;
	}
	else if (Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic)
	{
		BlockColor = WorkbenchSettings.DiagnosticBlockColor;
	}
	if (Block.bSelected)
	{
		BlockColor = WorkbenchSettings.SelectedBlockColor;
	}

	return SNew(SBox)
	.WidthOverride(WorkbenchSettings.PreviewWidth)
	.MinDesiredHeight(WorkbenchSettings.PreviewMinHeight)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(BlockColor)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Block.Title))
				.ColorAndOpacity(FLinearColor::White)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Block.Detail))
				.ColorAndOpacity(FLinearColor(0.86f, 0.90f, 0.95f, 1.0f))
				.AutoWrapText(true)
			]
		]
	];
}

UEdGraph* SBlueprintHelperTaskSpecWorkbench::GetCurrentTargetGraph() const
{
	if (GraphResolver)
	{
		return GraphResolver->GetFocusedGraph();
	}
	return FBlueprintHelperModule::Get().GetActiveBlueprintGraph();
}
