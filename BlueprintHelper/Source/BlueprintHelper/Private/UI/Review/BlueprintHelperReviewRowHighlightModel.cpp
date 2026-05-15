// BlueprintHelper Review row highlight model.

#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

TMap<FString, FBlueprintHelperReviewRowHighlightModel::FRowHighlightSurfaceState>&
FBlueprintHelperReviewRowHighlightModel::GetRowHighlightSurfaceStates()
{
	static TMap<FString, FRowHighlightSurfaceState> States;
	return States;
}

TSet<FString>& FBlueprintHelperReviewRowHighlightModel::GetEmittedRowHighlightDebugKeys()
{
	static TSet<FString> Keys;
	return Keys;
}

FString FBlueprintHelperReviewRowHighlightModel::NormalizeGeometrySearchText(FString Text)
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

void FBlueprintHelperReviewRowHighlightModel::AddGeometrySearchTerms(
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

bool FBlueprintHelperReviewRowHighlightModel::GeometrySearchTextMatches(
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

const TCHAR* FBlueprintHelperReviewRowHighlightModel::SurfaceDebugName(
	EBlueprintHelperReviewSurface Surface)
{
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Graph:       return TEXT("Graph");
	case EBlueprintHelperReviewSurface::Components:  return TEXT("Components");
	case EBlueprintHelperReviewSurface::MyBlueprint: return TEXT("MyBlueprint");
	case EBlueprintHelperReviewSurface::Details:     return TEXT("Details");
	case EBlueprintHelperReviewSurface::UMGWidgetTree: return TEXT("UMGWidgetTree");
	case EBlueprintHelperReviewSurface::DataTable:   return TEXT("DataTable");
	case EBlueprintHelperReviewSurface::DataAsset:   return TEXT("DataAsset");
	default:                                         return TEXT("Unknown");
	}
}

bool FBlueprintHelperReviewRowHighlightModel::IsSameChange(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right)
{
	return Left.IsValid() && Right.IsValid() && Left->ChangeId == Right->ChangeId;
}

FString FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightStateKey(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface)
{
	return FString::Printf(TEXT("%s|%s"), *AssetPath, BlueprintHelperReviewSurfaceToString(Surface));
}

FString FBlueprintHelperReviewRowHighlightModel::ExtractReadableTail(FString Text)
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

void FBlueprintHelperReviewRowHighlightModel::AddRowHighlightKey(
	const FString& Key,
	TArray<FString>& OutKeys)
{
	FString Trimmed = Key;
	Trimmed.TrimStartAndEndInline();
	if (!Trimmed.IsEmpty())
	{
		OutKeys.AddUnique(Trimmed);
		const FString Tail = ExtractReadableTail(Trimmed);
		if (!Tail.IsEmpty())
		{
			OutKeys.AddUnique(Tail);
		}
	}
}

FString FBlueprintHelperReviewRowHighlightModel::GetReviewListTargetText(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	EBlueprintHelperReviewSurface Surface)
{
	if (!Item.IsValid())
	{
		return FString();
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
	{
		if (Target.Surface != Surface)
		{
			continue;
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			return Target.PropertyPath;
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			return Target.ComponentPath;
		}
		if (!Target.TargetKey.IsEmpty())
		{
			return Target.TargetKey;
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			return Target.DisplayLabel;
		}
	}

	return Item->LocationKey.IsEmpty() ? Item->DisplayLabel : Item->LocationKey;
}

void FBlueprintHelperReviewRowHighlightModel::CollectRowHighlightKeys(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	TArray<FString>& OutKeys)
{
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		const bool bTargetMatchesSurface = Target.Surface == Surface;
		const bool bDetailsTargetMatches = Surface == EBlueprintHelperReviewSurface::Details
			&& BlueprintHelperReviewTargetKindCanRouteToDetails(Target.TargetKind)
			&& Target.Surface != EBlueprintHelperReviewSurface::DataAsset
			&& Target.Surface != EBlueprintHelperReviewSurface::DataTable
			&& Target.Surface != EBlueprintHelperReviewSurface::UMGWidgetTree;
		if (!bTargetMatchesSurface && !bDetailsTargetMatches)
		{
			continue;
		}

		AddRowHighlightKey(Target.TargetKey, OutKeys);
		AddRowHighlightKey(Target.PropertyPath, OutKeys);
		AddRowHighlightKey(Target.ComponentPath, OutKeys);
		AddRowHighlightKey(Target.DisplayLabel, OutKeys);
	}

	AddRowHighlightKey(GetReviewListTargetText(MakeShared<FBlueprintHelperReviewVisibleChange>(Change), Surface), OutKeys);
	AddRowHighlightKey(Change.LocationKey, OutKeys);
	AddRowHighlightKey(Change.DisplayLabel, OutKeys);
}

bool FBlueprintHelperReviewRowHighlightModel::FindRowHighlightEntry(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	FRowHighlightEntry& OutEntry)
{
	const FRowHighlightSurfaceState* State =
		GetRowHighlightSurfaceStates().Find(BuildRowHighlightStateKey(AssetPath, Surface));
	if (!State)
	{
		return false;
	}

	if (const FRowHighlightEntry* Exact = State->TargetKeyToHighlight.Find(SearchText))
	{
		OutEntry = *Exact;
		return true;
	}

	for (const TPair<FString, FRowHighlightEntry>& Pair : State->TargetKeyToHighlight)
	{
		if (GeometrySearchTextMatches(Pair.Key, SearchText)
			|| GeometrySearchTextMatches(SearchText, Pair.Key))
		{
			OutEntry = Pair.Value;
			return true;
		}
	}

	return false;
}

FReply FBlueprintHelperReviewRowHighlightModel::ExecuteHighlightedRowAction(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	bool bAccept)
{
	const FRowHighlightSurfaceState* State =
		GetRowHighlightSurfaceStates().Find(BuildRowHighlightStateKey(AssetPath, Surface));
	if (!State)
	{
		return FReply::Handled();
	}

	FRowHighlightEntry Entry;
	if (!FindRowHighlightEntry(AssetPath, Surface, SearchText, Entry) || !Entry.Change.IsValid())
	{
		return FReply::Handled();
	}

	if (bAccept)
	{
		return State->OnAcceptChange ? State->OnAcceptChange(Entry.Change) : FReply::Handled();
	}
	return State->OnRejectChange ? State->OnRejectChange(Entry.Change) : FReply::Handled();
}

FSlateColor FBlueprintHelperReviewRowHighlightModel::ResolveRowHighlightColor(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	FRowHighlightEntry Entry;
	if (!FindRowHighlightEntry(AssetPath, Surface, SearchText, Entry))
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	const FRowHighlightSurfaceState* State =
		GetRowHighlightSurfaceStates().Find(BuildRowHighlightStateKey(AssetPath, Surface));
	const FSlateColor SourceColor = State && State->GetChangeColor
		? State->GetChangeColor(Entry.ChangeKind)
		: FSlateColor(FLinearColor::Yellow);
	return FSlateColor(FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(
		SourceColor.GetSpecifiedColor()));
}

EVisibility FBlueprintHelperReviewRowHighlightModel::ResolveRowActionsVisibility(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	FRowHighlightEntry Entry;
	return FindRowHighlightEntry(AssetPath, Surface, SearchText, Entry) && Entry.bSelected
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

TSharedRef<SWidget> FBlueprintHelperReviewRowHighlightModel::BuildComponentRowHighlightFill(
	const FSlateColor& FillColor)
{
	return SNew(SBorder)
		.Visibility(EVisibility::HitTestInvisible)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.White")))
		.BorderBackgroundColor(FillColor)
		.Padding(0.0f)
		[
			SNullWidget::NullWidget
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewRowHighlightModel::BuildComponentRowActions(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(FMargin(4.0f, 2.0f))
		.Visibility_Lambda([AssetPath, Surface, SearchText]()
		{
			return FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
				AssetPath,
				Surface,
				SearchText);
		})
		[
			SNew(SHorizontalBox)
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
			]
		];
}

void FBlueprintHelperReviewRowHighlightModel::AddComponentRowOverlay(
	const TSharedPtr<SCanvas>& Canvas,
	const FBlueprintHelperReviewSurfaceGeometryAnchor& Anchor,
	const FSlateColor& FillColor,
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	bool bSelected)
{
	if (!Canvas.IsValid() || !Anchor.bIsValid)
	{
		return;
	}

	Canvas->AddSlot()
	.Position(Anchor.Position)
	.Size(Anchor.Size)
	[
		BuildComponentRowHighlightFill(FillColor)
	];

	if (!bSelected)
	{
		return;
	}

	const FVector2D ActionSize(144.0f, FMath::Max(24.0f, Anchor.Size.Y - 4.0f));
	const float ActionX = Anchor.Position.X + FMath::Max(4.0f, Anchor.Size.X - ActionSize.X - 4.0f);
	Canvas->AddSlot()
	.Position(FVector2D(ActionX, Anchor.Position.Y + 2.0f))
	.Size(ActionSize)
	[
		BuildComponentRowActions(AssetPath, Surface, SearchText)
	];
}

void FBlueprintHelperReviewRowHighlightModel::EmitDedupedRowHighlightDebug(
	const TFunction<void(const FString&)>& AddDebugMessage,
	const FString& Message,
	EBlueprintHelperReviewSurface Surface,
	const FString& ChangeId,
	const FString& Result,
	const FString& Reason)
{
	if (!AddDebugMessage)
	{
		return;
	}

	const FString Key = FString::Printf(
		TEXT("%s|%s|%s|%s"),
		BlueprintHelperReviewSurfaceToString(Surface),
		*ChangeId,
		*Result,
		*Reason);
	if (GetEmittedRowHighlightDebugKeys().Contains(Key))
	{
		return;
	}

	GetEmittedRowHighlightDebugKeys().Add(Key);
	AddDebugMessage(Message);
}

bool FBlueprintHelperReviewRowHighlightModel::IsRowHighlightSurface(EBlueprintHelperReviewSurface Surface)
{
	return Surface == EBlueprintHelperReviewSurface::Components
		|| Surface == EBlueprintHelperReviewSurface::MyBlueprint
		|| Surface == EBlueprintHelperReviewSurface::Details
		|| Surface == EBlueprintHelperReviewSurface::UMGWidgetTree
		|| Surface == EBlueprintHelperReviewSurface::DataTable
		|| Surface == EBlueprintHelperReviewSurface::DataAsset;
}

FLinearColor FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(const FLinearColor& ChangeColor)
{
	FLinearColor FillColor = ChangeColor == FLinearColor::Transparent
		? FLinearColor::Yellow
		: ChangeColor;
	FillColor.A = 0.6f;
	return FillColor;
}

TMap<FString, FBlueprintHelperReviewRowHighlight> FBlueprintHelperReviewRowHighlightModel::BuildTargetKeyToHighlight(
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
	EBlueprintHelperReviewSurface Surface,
	const FString& CurrentAssetPath)
{
	TMap<FString, FBlueprintHelperReviewRowHighlight> TargetKeyToHighlight;
	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}
		if (!FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(*Item, Surface))
		{
			continue;
		}

		TArray<FString> Keys;
		CollectRowHighlightKeys(*Item, Surface, Keys);
		for (const FString& Key : Keys)
		{
			FBlueprintHelperReviewRowHighlight Highlight;
			Highlight.ChangeId = Item->ChangeId;
			Highlight.TargetKey = Key;
			Highlight.ChangeKind = Item->ChangeKind;
			Highlight.bSelected = IsSameChange(Item, SelectedChange);
			TargetKeyToHighlight.Add(Key, Highlight);
		}
	}

	return TargetKeyToHighlight;
}

TSharedRef<SWidget> FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	EBlueprintHelperReviewSurface Surface,
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	if (!IsRowHighlightSurface(Surface) || !Args.ChangeItems)
	{
		return SNullWidget::NullWidget;
	}

	const FString ContextAssetPath = Args.AssetContext ? Args.AssetContext->AssetPath : FString();
	const FString SelectedAssetPath = Args.SelectedChange.IsValid() ? Args.SelectedChange->AssetPath : FString();
	const FString CurrentAssetPath = ContextAssetPath.IsEmpty() ? SelectedAssetPath : ContextAssetPath;
	const FString FilterAssetPath = SelectedAssetPath.IsEmpty() ? CurrentAssetPath : SelectedAssetPath;
	FRowHighlightSurfaceState State;
	State.AssetPath = CurrentAssetPath;
	State.Surface = Surface;
	State.OnAcceptChange = Args.OnAcceptChange;
	State.OnRejectChange = Args.OnRejectChange;
	State.GetChangeColor = Args.GetChangeColor;

	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> HighlightedItems;
	TMap<FString, FString> PrimaryTargetByChangeId;

	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : *Args.ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if ((!CurrentAssetPath.IsEmpty() || !FilterAssetPath.IsEmpty())
			&& Item->AssetPath != CurrentAssetPath
			&& Item->AssetPath != FilterAssetPath)
		{
			continue;
		}

		const FBlueprintHelperReviewSurfaceRouteDecision RouteDecision =
			FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(*Item, Surface);
		if (Args.AddDebugMessage)
		{
			const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
				? Args.AssetContext->AssetKind
				: EBlueprintHelperReviewAssetKind::Unknown;
			EmitDedupedRowHighlightDebug(
				Args.AddDebugMessage,
				FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
					*Item,
					Surface,
					RouteDecision,
					BlueprintHelperReviewAssetKindToString(AssetKind)),
				Surface,
				Item->ChangeId,
				RouteDecision.bShouldShow ? TEXT("shown") : TEXT("hidden"),
				RouteDecision.Reason);
		}
		if (!RouteDecision.bShouldShow || !Predicate(*Item))
		{
			continue;
		}

		TArray<FString> Keys;
		CollectRowHighlightKeys(*Item, Surface, Keys);
		if (Keys.Num() == 0)
		{
			continue;
		}

		FString PrimaryTarget = GetReviewListTargetText(Item, Surface);
		if (PrimaryTarget.IsEmpty())
		{
			PrimaryTarget = Keys[0];
		}
		PrimaryTargetByChangeId.Add(Item->ChangeId, PrimaryTarget);

		for (const FString& Key : Keys)
		{
			FRowHighlightEntry Entry;
			Entry.ChangeId = Item->ChangeId;
			Entry.TargetKey = Key;
			Entry.ChangeKind = Item->ChangeKind;
			Entry.bSelected = IsSameChange(Item, Args.SelectedChange);
			Entry.Change = Item;
			State.TargetKeyToHighlight.Add(Key, Entry);
		}
		HighlightedItems.Add(Item);
	}

	GetRowHighlightSurfaceStates().Add(
		BuildRowHighlightStateKey(CurrentAssetPath, Surface),
		State);
	if (!FilterAssetPath.IsEmpty() && FilterAssetPath != CurrentAssetPath)
	{
		GetRowHighlightSurfaceStates().Add(
			BuildRowHighlightStateKey(FilterAssetPath, Surface),
			State);
	}
	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : HighlightedItems)
	{
		if (Item.IsValid() && !Item->AssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			GetRowHighlightSurfaceStates().Add(
				BuildRowHighlightStateKey(Item->AssetPath, Surface),
				State);
		}
	}

	TSharedPtr<SCanvas> DetailsOverlayCanvas;
	bool bHasDetailsOverlay = false;
	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : HighlightedItems)
	{
		FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;
		const bool bResolved = Args.ResolveRowGeometry.IsBound()
			&& Args.ResolveRowGeometry.Execute(*Item, Surface, Anchor)
			&& Anchor.bIsValid;
		const FString TargetText = PrimaryTargetByChangeId.FindRef(Item->ChangeId);

		if (bResolved)
		{
			if (Surface == EBlueprintHelperReviewSurface::Details)
			{
				if (!DetailsOverlayCanvas.IsValid())
				{
					DetailsOverlayCanvas = SNew(SCanvas);
				}

				const FSlateColor SourceColor = State.GetChangeColor
					? State.GetChangeColor(Item->ChangeKind)
					: FSlateColor(FLinearColor::Yellow);
				const FSlateColor FillColor(GetRowHighlightFillColor(SourceColor.GetSpecifiedColor()));
				const FString OverlayAssetPath = Item->AssetPath.IsEmpty() ? CurrentAssetPath : Item->AssetPath;
				AddComponentRowOverlay(
					DetailsOverlayCanvas,
					Anchor,
					FillColor,
					OverlayAssetPath,
					Surface,
					TargetText,
					IsSameChange(Item, Args.SelectedChange));
				bHasDetailsOverlay = true;
			}
			EmitDedupedRowHighlightDebug(
				Args.AddDebugMessage,
				FString::Printf(
					TEXT("ReviewRowHighlight change=%s surface=%s target=\"%s\" result=shown mode=row_background"),
					*Item->ChangeId,
					SurfaceDebugName(Surface),
					*TargetText),
				Surface,
				Item->ChangeId,
				TEXT("shown"),
				TEXT("row_background"));
			continue;
		}

		const FString Reason = Anchor.Reason.IsEmpty() ? TEXT("row_not_visible") : Anchor.Reason;
		EmitDedupedRowHighlightDebug(
			Args.AddDebugMessage,
			FString::Printf(
				TEXT("ReviewRowHighlight change=%s surface=%s target=\"%s\" result=pending reason=%s"),
				*Item->ChangeId,
				SurfaceDebugName(Surface),
				*TargetText,
				*Reason),
			Surface,
			Item->ChangeId,
			TEXT("pending"),
			Reason);
	}

	if (bHasDetailsOverlay && DetailsOverlayCanvas.IsValid())
	{
		return DetailsOverlayCanvas.ToSharedRef();
	}
	return SNullWidget::NullWidget;
}

FSlateColor FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	return ResolveRowHighlightColor(
		AssetPath,
		Surface,
		SearchText);
}

EVisibility FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	return ResolveRowActionsVisibility(
		AssetPath,
		Surface,
		SearchText);
}

FReply FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	return ExecuteHighlightedRowAction(
		AssetPath,
		Surface,
		SearchText,
		true);
}

FReply FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	return ExecuteHighlightedRowAction(
		AssetPath,
		Surface,
		SearchText,
		false);
}
