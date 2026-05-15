// BlueprintHelper Review MyBlueprint presenter.

#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"

#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "UI/Review/Native/MyBlueprint/SBlueprintHelperReviewMyBlueprintPanel.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
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

	auto MakeVariableRow = [](const FString& Label, const FString& SearchText, ERowKind Kind, const FEdGraphPinType& PinType)
	{
		TSharedRef<FRowItem> Row = MakeShared<FRowItem>();
		Row->Label = FText::FromString(Label);
		Row->SearchText = SearchText;
		Row->Kind = Kind;
		Row->PinType = PinType;
		Row->bHasPinType = true;
		return Row;
	};

	auto AddSection = [&State, &MakeRow](const TCHAR* Label)
	{
		TSharedRef<FRowItem> Section = MakeRow(Label, FString(), ERowKind::Section);
		State.RootItems.Add(Section);
		return Section;
	};

	auto AddChildRow = [&MakeRow](
		const TSharedRef<FRowItem>& Section,
		const FString& Label,
		const FString& SearchText,
		ERowKind Kind)
	{
		TSharedRef<FRowItem> Row = MakeRow(Label, SearchText, Kind);
		Section->Children.Add(Row);
		return Row;
	};

	auto AddVariableChildRow = [&MakeVariableRow](
		const TSharedRef<FRowItem>& Section,
		const FString& Label,
		const FString& SearchText,
		ERowKind Kind,
		const FEdGraphPinType& PinType)
	{
		TSharedRef<FRowItem> Row = MakeVariableRow(Label, SearchText, Kind, PinType);
		Section->Children.Add(Row);
		return Row;
	};

	auto AddGraphRows = [&AddChildRow](
		const TSharedRef<FRowItem>& Section,
		const TArray<TObjectPtr<UEdGraph>>& Graphs,
		ERowKind Kind)
	{
		TArray<TSharedRef<FRowItem>> Rows;
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			const FString GraphName = Graph->GetName();
			Rows.Add(AddChildRow(Section, GraphName, GraphName, Kind));
		}
		return Rows;
	};

	TSharedRef<FRowItem> GraphSection = AddSection(TEXT("Graphs"));
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		const FString GraphName = Graph->GetName();
		TSharedRef<FRowItem> GraphRow = AddChildRow(GraphSection, GraphName, GraphName, ERowKind::Graph);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (!CustomEvent)
			{
				continue;
			}

			const FString EventName = CustomEvent->CustomFunctionName.ToString();
			if (!EventName.IsEmpty())
			{
				GraphRow->Children.Add(MakeRow(EventName, EventName, ERowKind::Event));
			}
		}
	}

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
			AddVariableChildRow(DispatcherSection, VariableName, VariableName, ERowKind::Dispatcher, Variable.VarType);
		}
		else
		{
			AddVariableChildRow(VariableSection, VariableName, VariableName, ERowKind::Variable, Variable.VarType);
		}
	}

	TSharedRef<SBlueprintHelperReviewMyBlueprintPanel> Panel = SNew(SBlueprintHelperReviewMyBlueprintPanel)
		.RootItemsSource(&State.RootItems)
		.AssetPath(Context.AssetPath)
		.OnGeometryInvalidated(OnGeometryInvalidated);
	State.TreeView = Panel->GetTreeView();
	return Panel;
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
