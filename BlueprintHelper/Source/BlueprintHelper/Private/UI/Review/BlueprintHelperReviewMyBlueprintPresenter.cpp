// BlueprintHelper Review MyBlueprint presenter.

#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"

#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FBlueprintHelperReviewMyBlueprintPresenterUtils
{
public:
	static TSharedRef<SWidget> BuildReviewPlaceholder(const FString& Message);

	static FString ExtractReadableTail(FString Text);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);

	static TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> FindMyBlueprintRowByText(
		const TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& Rows,
		const FString& TargetText);

	static bool BuildGeometryAnchorFromRowWidget(
		const TSharedPtr<SWidget>& RowWidget,
		const TSharedPtr<SWidget>& OverlayWidget,
		const FString& TargetText,
		const TCHAR* DebugMode,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);

private:
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
};

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenterUtils::BuildReviewPlaceholder(const FString& Message)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
			.AutoWrapText(true)
			.Text(FText::FromString(Message))
		];
}

FString FBlueprintHelperReviewMyBlueprintPresenterUtils::NormalizeGeometrySearchText(FString Text)
{
	Text.ToLowerInline();
	for (int32 Index = Text.Len() - 1; Index >= 0; --Index)
	{
		const TCHAR Character = Text[Index];
		if (!FChar::IsAlnum(Character))
		{
			Text.RemoveAt(Index);
		}
	}
	return Text;
}

void FBlueprintHelperReviewMyBlueprintPresenterUtils::AddGeometrySearchTerms(
	const FString& RawText,
	TArray<FString>& OutTerms)
{
	OutTerms.AddUnique(NormalizeGeometrySearchText(RawText));
	FString CurrentPart;
	for (int32 Index = 0; Index < RawText.Len(); ++Index)
	{
		const TCHAR Character = RawText[Index];
		if (FChar::IsAlnum(Character))
		{
			CurrentPart.AppendChar(Character);
			continue;
		}

		const FString Term = NormalizeGeometrySearchText(CurrentPart);
		if (Term.Len() >= 2)
		{
			OutTerms.AddUnique(Term);
		}
		CurrentPart.Reset();
	}

	const FString TailTerm = NormalizeGeometrySearchText(CurrentPart);
	if (TailTerm.Len() >= 2)
	{
		OutTerms.AddUnique(TailTerm);
	}
}

bool FBlueprintHelperReviewMyBlueprintPresenterUtils::GeometrySearchTextMatches(
	const FString& RowSearchText,
	const FString& TargetText)
{
	const FString NormalizedRow = NormalizeGeometrySearchText(RowSearchText);
	if (NormalizedRow.IsEmpty())
	{
		return false;
	}

	TArray<FString> TargetTerms;
	AddGeometrySearchTerms(TargetText, TargetTerms);
	if (TargetTerms.Num() == 0)
	{
		return false;
	}

	for (const FString& TargetTerm : TargetTerms)
	{
		if (TargetTerm.Len() < 2)
		{
			continue;
		}
		if (NormalizedRow.Contains(TargetTerm) || TargetTerm.Contains(NormalizedRow))
		{
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperReviewMyBlueprintPresenterUtils::ExtractReadableTail(FString Text)
{
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return Text;
	}

	int32 DelimiterIndex = INDEX_NONE;
	if (Text.FindLastChar(TEXT(':'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('/'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('.'), DelimiterIndex))
	{
		Text = Text.Mid(DelimiterIndex + 1);
	}
	Text.TrimStartAndEndInline();

	if (Text.EndsWith(TEXT(" Widget"), ESearchCase::IgnoreCase))
	{
		Text.LeftChopInline(7);
	}
	if (Text.EndsWith(TEXT(" Row"), ESearchCase::IgnoreCase))
	{
		Text.LeftChopInline(4);
	}
	Text.TrimStartAndEndInline();
	return Text;
}

TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>
FBlueprintHelperReviewMyBlueprintPresenterUtils::FindMyBlueprintRowByText(
	const TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& Rows,
	const FString& TargetText)
{
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Row : Rows)
	{
		if (!Row.IsValid())
		{
			continue;
		}

		if (!Row->SearchText.IsEmpty()
			&& GeometrySearchTextMatches(Row->SearchText, TargetText))
		{
			return Row;
		}

		TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> ChildMatch =
			FindMyBlueprintRowByText(Row->Children, TargetText);
		if (ChildMatch.IsValid())
		{
			return ChildMatch;
		}
	}
	return nullptr;
}

bool FBlueprintHelperReviewMyBlueprintPresenterUtils::BuildGeometryAnchorFromRowWidget(
	const TSharedPtr<SWidget>& RowWidget,
	const TSharedPtr<SWidget>& OverlayWidget,
	const FString& TargetText,
	const TCHAR* DebugMode,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!RowWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
		OutAnchor.TargetText = TargetText;
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		OutAnchor.TargetText = TargetText;
		return false;
	}

	const FGeometry& RowGeometry = RowWidget->GetTickSpaceGeometry();
	const FGeometry& OverlayGeometry = OverlayWidget->GetTickSpaceGeometry();
	const FVector2D RowLocalSize = RowGeometry.GetLocalSize();
	const FVector2D OverlayLocalSize = OverlayGeometry.GetLocalSize();
	if (RowLocalSize.X <= 0.0f || RowLocalSize.Y <= 0.0f
		|| OverlayLocalSize.X <= 0.0f || OverlayLocalSize.Y <= 0.0f)
	{
		OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
		OutAnchor.TargetText = TargetText;
		return false;
	}

	const FVector2D AbsoluteTopLeft = RowGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D AbsoluteBottomRight = RowGeometry.LocalToAbsolute(RowLocalSize);
	const FVector2D LocalTopLeft = OverlayGeometry.AbsoluteToLocal(AbsoluteTopLeft);
	const FVector2D LocalBottomRight = OverlayGeometry.AbsoluteToLocal(AbsoluteBottomRight);
	const FVector2D LocalSize = LocalBottomRight - LocalTopLeft;
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
		OutAnchor.TargetText = TargetText;
		return false;
	}

	OutAnchor.bIsValid = true;
	OutAnchor.Position = LocalTopLeft;
	OutAnchor.Size = LocalSize;
	OutAnchor.HostSize = OverlayLocalSize;
	OutAnchor.TargetText = TargetText;
	OutAnchor.Reason = TEXT("stable_slate_row_geometry");
	OutAnchor.DebugMode = DebugMode ? DebugMode : TEXT("slate_row");
	return true;
}

bool FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::MyBlueprint);
}

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FState& State,
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.RootItems.Reset();
	State.TreeView.Reset();
	State.OnGeometryInvalidated = OnGeometryInvalidated;
	(void)ChangeItems;

	UBlueprint* Blueprint = Context.Blueprint.Get();
	if (!Blueprint)
	{
		return FBlueprintHelperReviewMyBlueprintPresenterUtils::BuildReviewPlaceholder(
			TEXT("No Blueprint outline loaded."));
	}

	using FRowItem = FBlueprintHelperReviewMyBlueprintPresenter::FRowItem;
	using ERowKind = FBlueprintHelperReviewMyBlueprintPresenter::ERowKind;

	auto MakeRow = [](const FString& Label, const FString& SearchText, ERowKind Kind)
	{
		TSharedRef<FRowItem> Row = MakeShared<FRowItem>();
		Row->Label = FText::FromString(Label);
		Row->SearchText = SearchText;
		Row->Kind = Kind;
		return Row;
	};

	auto AddSection = [&State, &MakeRow](const TCHAR* Label)
	{
		TSharedRef<FRowItem> Section = MakeRow(Label, FString(), ERowKind::Section);
		State.RootItems.Add(Section);
		return Section;
	};

	TArray<FString> KnownRowSearchTexts;
	auto RememberRowSearchText = [&KnownRowSearchTexts](const FString& SearchText)
	{
		if (!SearchText.IsEmpty())
		{
			KnownRowSearchTexts.AddUnique(SearchText);
			const FString Tail = FBlueprintHelperReviewMyBlueprintPresenterUtils::ExtractReadableTail(SearchText);
			if (!Tail.IsEmpty())
			{
				KnownRowSearchTexts.AddUnique(Tail);
			}
		}
	};

	auto AddChildRow = [&MakeRow, &RememberRowSearchText](
		const TSharedRef<FRowItem>& Section,
		const FString& Label,
		const FString& SearchText,
		ERowKind Kind)
	{
		Section->Children.Add(MakeRow(Label, SearchText, Kind));
		RememberRowSearchText(SearchText);
	};

	auto AddGraphRows = [&AddChildRow](
		const TSharedRef<FRowItem>& Section,
		const TArray<TObjectPtr<UEdGraph>>& Graphs,
		ERowKind Kind)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			const FString GraphName = Graph->GetName();
			AddChildRow(Section, GraphName, GraphName, Kind);
		}
	};

	TSharedRef<FRowItem> GraphSection = AddSection(TEXT("Graphs"));
	AddGraphRows(GraphSection, Blueprint->UbergraphPages, ERowKind::Graph);
	TSharedRef<FRowItem> FunctionSection = AddSection(TEXT("Functions"));
	AddGraphRows(FunctionSection, Blueprint->FunctionGraphs, ERowKind::Function);
	TSharedRef<FRowItem> MacroSection = AddSection(TEXT("Macros"));
	AddGraphRows(MacroSection, Blueprint->MacroGraphs, ERowKind::Macro);

	TSharedRef<FRowItem> VariableSection = AddSection(TEXT("Variables"));
	TSharedRef<FRowItem> DispatcherSection = AddSection(TEXT("Event Dispatchers"));
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		const FString VariableName = Variable.VarName.ToString();
		if (VariableName.IsEmpty())
		{
			continue;
		}
		const bool bIsDispatcher = Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate;
		if (bIsDispatcher)
		{
			AddChildRow(DispatcherSection, VariableName, VariableName, ERowKind::Dispatcher);
		}
		else
		{
			AddChildRow(VariableSection, VariableName, VariableName, ERowKind::Variable);
		}
	}

	AddGraphRows(DispatcherSection, Blueprint->DelegateSignatureGraphs, ERowKind::Dispatcher);

	auto HasKnownRow = [&KnownRowSearchTexts](const FString& Candidate)
	{
		if (Candidate.IsEmpty())
		{
			return true;
		}
		for (const FString& Known : KnownRowSearchTexts)
		{
			if (FBlueprintHelperReviewMyBlueprintPresenterUtils::GeometrySearchTextMatches(Known, Candidate)
				|| FBlueprintHelperReviewMyBlueprintPresenterUtils::GeometrySearchTextMatches(Candidate, Known))
			{
				return true;
			}
		}
		return false;
	};

	TSharedPtr<FRowItem> ReviewAnchorSection;
	auto EnsureReviewAnchorSection = [&State, &MakeRow, &ReviewAnchorSection]()
	{
		if (!ReviewAnchorSection.IsValid())
		{
			ReviewAnchorSection = MakeRow(TEXT("Review Anchors"), FString(), ERowKind::Section);
			State.RootItems.Add(ReviewAnchorSection.ToSharedRef());
		}
		return ReviewAnchorSection.ToSharedRef();
	};

	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change : ChangeItems)
	{
		if (!Change.IsValid() || !ShouldShowChange(*Change))
		{
			continue;
		}
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change->AtomicTargets)
		{
			if (Target.Surface != EBlueprintHelperReviewSurface::MyBlueprint)
			{
				continue;
			}
			const FString Candidate = !Target.TargetKey.IsEmpty()
				? Target.TargetKey
				: (!Target.DisplayLabel.IsEmpty() ? Target.DisplayLabel : Change->DisplayLabel);
			const FString Label = FBlueprintHelperReviewMyBlueprintPresenterUtils::ExtractReadableTail(Candidate);
			if (Label.IsEmpty() || HasKnownRow(Candidate) || HasKnownRow(Label))
			{
				continue;
			}
			AddChildRow(EnsureReviewAnchorSection(), Label, Candidate, ERowKind::ReviewOnly);
		}
	}

	State.RootItems.RemoveAll([](const TSharedPtr<FRowItem>& Item)
	{
		return Item.IsValid()
			&& Item->Children.Num() == 0
			&& Item->Label.ToString() != TEXT("Event Dispatchers");
	});

	if (State.RootItems.Num() == 0)
	{
		return FBlueprintHelperReviewMyBlueprintPresenterUtils::BuildReviewPlaceholder(
			TEXT("No Blueprint outline loaded."));
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<STreeView<TSharedPtr<FRowItem>>> Tree = SAssignNew(State.TreeView, STreeView<TSharedPtr<FRowItem>>)
		.TreeItemsSource(&State.RootItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			const bool bIsSection = Item.IsValid() && Item->Kind == ERowKind::Section;
			const FSlateColor TextColor = bIsSection
				? FSlateColor(FLinearColor(0.84f, 0.84f, 0.84f, 1.0f))
				: FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f));
			const FSlateColor SectionBackgroundColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
			const FString SearchText = Item.IsValid() ? Item->SearchText : FString();
			TSharedRef<SWidget> RowContent =
				SNew(SBlueprintHelperReviewGeometryProbe)
				.Surface(EBlueprintHelperReviewSurface::MyBlueprint)
				.TargetKey(SearchText)
				.OnGeometryInvalidated(OnGeometryInvalidated)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
					.BorderBackgroundColor_Lambda([AssetPath, SearchText, bIsSection, SectionBackgroundColor]()
					{
						if (bIsSection)
						{
							return SectionBackgroundColor;
						}
						return FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
							AssetPath,
							EBlueprintHelperReviewSurface::MyBlueprint,
							SearchText);
					})
					.Padding(FMargin(4.0f, 2.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
							.ColorAndOpacity(TextColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							.Visibility_Lambda([AssetPath, SearchText, bIsSection]()
							{
								if (bIsSection)
								{
									return EVisibility::Collapsed;
								}
								return FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
									AssetPath,
									EBlueprintHelperReviewSurface::MyBlueprint,
									SearchText);
							})
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SButton)
								.Text(FText::FromString(TEXT("Accept")))
								.OnClicked_Lambda([AssetPath, SearchText]()
								{
									return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
										AssetPath,
										EBlueprintHelperReviewSurface::MyBlueprint,
										SearchText);
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.Text(FText::FromString(TEXT("Reject")))
								.OnClicked_Lambda([AssetPath, SearchText]()
								{
									return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
										AssetPath,
										EBlueprintHelperReviewSurface::MyBlueprint,
										SearchText);
								})
							]
						]
					]
				];
			TSharedRef<STableRow<TSharedPtr<FRowItem>>> RowWidget =
				SNew(STableRow<TSharedPtr<FRowItem>>, OwnerTable)
				.Padding(FMargin(2.0f, 1.0f))
				[
					RowContent
				];

			if (Item.IsValid())
			{
				Item->RowWidget = RowContent;
				if (!SearchText.IsEmpty())
				{
					FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
						AssetPath,
						EBlueprintHelperReviewSurface::MyBlueprint,
						SearchText,
						RowContent,
						TEXT("owned_tree_row"));
				}
			}
			return RowWidget;
		})
		.OnGetChildren_Lambda([](TSharedPtr<FRowItem> Item, TArray<TSharedPtr<FRowItem>>& OutChildren)
		{
			if (Item.IsValid())
			{
				OutChildren.Append(Item->Children);
			}
		});

	for (const TSharedPtr<FRowItem>& Root : State.RootItems)
	{
		State.TreeView->SetItemExpansion(Root, true);
	}

	return SNew(SBorder)
		.Padding(6.0f)
		[
			Tree
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewMyBlueprintPresenter::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	FState& State,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	const FString TargetText = FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
		Change,
		EBlueprintHelperReviewSurface::MyBlueprint);
	if (TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const TSharedPtr<FRowItem> Row = FBlueprintHelperReviewMyBlueprintPresenterUtils::FindMyBlueprintRowByText(
		State.RootItems,
		TargetText);
	if (!Row.IsValid())
	{
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("no_matching_my_blueprint_row");
		return false;
	}

	TSharedPtr<SWidget> RowWidget = Row->RowWidget.Pin();
	if (!RowWidget.IsValid())
	{
		if (State.TreeView.IsValid())
		{
			State.TreeView->RequestScrollIntoView(Row);
		}
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
		return false;
	}

	return FBlueprintHelperReviewMyBlueprintPresenterUtils::BuildGeometryAnchorFromRowWidget(
		RowWidget.ToSharedRef(),
		OverlayWidget,
		TargetText,
		TEXT("owned_tree_row"),
		OutAnchor);
}
