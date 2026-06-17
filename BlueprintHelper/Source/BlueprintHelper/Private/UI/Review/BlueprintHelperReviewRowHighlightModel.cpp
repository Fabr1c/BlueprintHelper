// BlueprintHelper Review row highlight model.

#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewGeometrySearchService.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
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

FBlueprintHelperReviewRowHighlightStateChanged&
FBlueprintHelperReviewRowHighlightModel::GetStateChangedDelegate()
{
	static FBlueprintHelperReviewRowHighlightStateChanged Delegate;
	return Delegate;
}

uint64& FBlueprintHelperReviewRowHighlightModel::GetStateRevisionCounter()
{
	static uint64 Revision = 0;
	return Revision;
}

uint64 FBlueprintHelperReviewRowHighlightModel::NextStateRevision()
{
	uint64& Revision = GetStateRevisionCounter();
	++Revision;
	return Revision;
}

FDelegateHandle FBlueprintHelperReviewRowHighlightModel::AddStateChangedHandler(
	FBlueprintHelperReviewRowHighlightStateChanged::FDelegate InDelegate)
{
	return GetStateChangedDelegate().Add(MoveTemp(InDelegate));
}

void FBlueprintHelperReviewRowHighlightModel::RemoveStateChangedHandler(FDelegateHandle InHandle)
{
	if (InHandle.IsValid())
	{
		GetStateChangedDelegate().Remove(InHandle);
	}
}

void FBlueprintHelperReviewRowHighlightModel::BroadcastStateChanged(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	uint64 Revision)
{
	if (!IsRowHighlightSurface(Surface))
	{
		return;
	}
	GetStateChangedDelegate().Broadcast(AssetPath, Surface, Revision);
}

FString FBlueprintHelperReviewRowHighlightModel::NormalizeGeometrySearchText(FString Text)
{
	return FBlueprintHelperReviewGeometrySearchService::NormalizeSearchText(MoveTemp(Text));
}

void FBlueprintHelperReviewRowHighlightModel::AddGeometrySearchTerms(
	const FString& RawText,
	TArray<FString>& OutTerms)
{
	FBlueprintHelperReviewGeometrySearchService::AddGeometrySearchTerms(RawText, OutTerms);
}

bool FBlueprintHelperReviewRowHighlightModel::GeometrySearchTextMatches(
	const FString& RowSearchText,
	const FString& TargetText)
{
	return FBlueprintHelperReviewGeometrySearchService::GeometrySearchTextMatches(RowSearchText, TargetText);
}

const TCHAR* FBlueprintHelperReviewRowHighlightModel::SurfaceDebugName(
	EBlueprintHelperReviewSurface Surface)
{
	struct FBlueprintHelperReviewSurfaceDebugName
	{
		EBlueprintHelperReviewSurface Surface;
		const TCHAR* Name;
	};

	static const FBlueprintHelperReviewSurfaceDebugName SurfaceNames[] =
	{
		{ EBlueprintHelperReviewSurface::Graph, TEXT("Graph") },
		{ EBlueprintHelperReviewSurface::Components, TEXT("Components") },
		{ EBlueprintHelperReviewSurface::MyBlueprint, TEXT("MyBlueprint") },
		{ EBlueprintHelperReviewSurface::Details, TEXT("Details") },
		{ EBlueprintHelperReviewSurface::UMGWidgetTree, TEXT("UMGWidgetTree") },
		{ EBlueprintHelperReviewSurface::DataTable, TEXT("DataTable") },
		{ EBlueprintHelperReviewSurface::DataAsset, TEXT("DataAsset") },
		{ EBlueprintHelperReviewSurface::Material, TEXT("Material") }
	};

	for (const FBlueprintHelperReviewSurfaceDebugName& Entry : SurfaceNames)
	{
		if (Entry.Surface == Surface)
		{
			return Entry.Name;
		}
	}
	return TEXT("Unknown");
}

bool FBlueprintHelperReviewRowHighlightModel::IsSameChange(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right)
{
	return Left.IsValid() && Right.IsValid() && Left->ChangeId == Right->ChangeId;
}

bool FBlueprintHelperReviewRowHighlightModel::AreBindingsEquivalent(
	const FBlueprintHelperReviewRowBinding& Left,
	const FBlueprintHelperReviewRowBinding& Right)
{
	return Left.AssetPath == Right.AssetPath
		&& Left.ChangeId == Right.ChangeId
		&& Left.AtomicTargetId == Right.AtomicTargetId
		&& Left.TargetKey == Right.TargetKey
		&& Left.Surface == Right.Surface;
}

bool FBlueprintHelperReviewRowHighlightModel::AreEntriesEquivalent(
	const FRowHighlightEntry& Left,
	const FRowHighlightEntry& Right)
{
	return Left.ChangeId == Right.ChangeId
		&& Left.TargetKey == Right.TargetKey
		&& Left.ChangeKind == Right.ChangeKind
		&& Left.bSelected == Right.bSelected
		&& Left.DiffModel.TargetKind == Right.DiffModel.TargetKind
		&& Left.DiffModel.TargetKey == Right.DiffModel.TargetKey
		&& Left.DiffModel.DisplayLabel == Right.DiffModel.DisplayLabel
		&& Left.DiffModel.DiffColor == Right.DiffModel.DiffColor
		&& AreBindingsEquivalent(Left.Binding, Right.Binding);
}

bool FBlueprintHelperReviewRowHighlightModel::AreSurfaceStatesEquivalent(
	const FRowHighlightSurfaceState& Left,
	const FRowHighlightSurfaceState& Right)
{
	if (Left.AssetPath != Right.AssetPath
		|| Left.Surface != Right.Surface
		|| Left.TargetKeyToHighlight.Num() != Right.TargetKeyToHighlight.Num())
	{
		return false;
	}

	for (const TPair<FString, FRowHighlightEntry>& Pair : Left.TargetKeyToHighlight)
	{
		const FRowHighlightEntry* RightEntry = Right.TargetKeyToHighlight.Find(Pair.Key);
		if (!RightEntry || !AreEntriesEquivalent(Pair.Value, *RightEntry))
		{
			return false;
		}
	}
	return true;
}

FString FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightStateKey(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface)
{
	return FString::Printf(TEXT("%s|%s"), *AssetPath, BlueprintHelperReviewSurfaceToString(Surface));
}

void FBlueprintHelperReviewRowHighlightModel::AddStateAssetPath(
	const FString& AssetPath,
	TArray<FString>& OutAssetPaths)
{
	FString TrimmedAssetPath = AssetPath;
	TrimmedAssetPath.TrimStartAndEndInline();
	if (!TrimmedAssetPath.IsEmpty())
	{
		OutAssetPaths.AddUnique(TrimmedAssetPath);
	}
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

void FBlueprintHelperReviewRowHighlightModel::CollectProjectionModelKeys(
	const FBlueprintHelperReviewSurfaceDiffProjectionModel& DiffModel,
	TArray<FString>& OutKeys)
{
	for (const FString& MatchKey : DiffModel.MatchKeys)
	{
		AddRowHighlightKey(MatchKey, OutKeys);
	}
	AddRowHighlightKey(DiffModel.TargetKey, OutKeys);
	AddRowHighlightKey(DiffModel.DisplayLabel, OutKeys);
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
			&& Target.Surface != EBlueprintHelperReviewSurface::UMGWidgetTree
			&& Target.Surface != EBlueprintHelperReviewSurface::Material;
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
	FRowHighlightEntry& OutEntry,
	bool bAllowFuzzyMatch)
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

	if (!bAllowFuzzyMatch)
	{
		return false;
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

bool FBlueprintHelperReviewRowHighlightModel::FindExactRowHighlightEntry(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	FRowHighlightEntry& OutEntry)
{
	if (FindRowHighlightEntry(AssetPath, Surface, SearchText, OutEntry, false))
	{
		return true;
	}

	TArray<FString> LookupTokens;
	SearchText.ParseIntoArrayWS(LookupTokens);
	for (const FString& LookupToken : LookupTokens)
	{
		if (LookupToken.IsEmpty() || LookupToken == SearchText)
		{
			continue;
		}
		if (FindRowHighlightEntry(AssetPath, Surface, LookupToken, OutEntry, false))
		{
			return true;
		}
	}

	return false;
}

bool FBlueprintHelperReviewRowHighlightModel::TryGetRowActionBinding(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	FBlueprintHelperReviewRowBinding& OutBinding)
{
	FRowHighlightEntry Entry;
	if (!FindExactRowHighlightEntry(AssetPath, Surface, SearchText, Entry))
	{
		return false;
	}
	OutBinding = Entry.Binding;
	return OutBinding.IsValid();
}

FReply FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	EBlueprintHelperReviewActionIntentKind Action,
	const FString& SourceWidget)
{
	const FRowHighlightSurfaceState* State =
		GetRowHighlightSurfaceStates().Find(BuildRowHighlightStateKey(AssetPath, Surface));
	if (!State || !State->OnReviewActionIntent)
	{
		return FReply::Handled();
	}

	FBlueprintHelperReviewRowBinding Binding;
	if (!TryGetRowActionBinding(AssetPath, Surface, SearchText, Binding))
	{
		return FReply::Handled();
	}

	const FBlueprintHelperReviewActionIntent Intent = Action == EBlueprintHelperReviewActionIntentKind::Accept
		? FBlueprintHelperReviewActionIntent::Accept(Binding, SourceWidget)
		: FBlueprintHelperReviewActionIntent::Reject(Binding, SourceWidget);
	return State->OnReviewActionIntent(Intent);
}

FSlateColor FBlueprintHelperReviewRowHighlightModel::ResolveRowHighlightColor(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	FRowHighlightEntry Entry;
	if (!FindExactRowHighlightEntry(AssetPath, Surface, SearchText, Entry))
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	const FRowHighlightSurfaceState* State =
		GetRowHighlightSurfaceStates().Find(BuildRowHighlightStateKey(AssetPath, Surface));
	const FSlateColor SourceColor = State && State->GetChangeColor
		? State->GetChangeColor(Entry.ChangeKind)
		: FSlateColor(FLinearColor::Yellow);
	if (Entry.DiffModel.DiffColor.A > 0.0f)
	{
		return FSlateColor(FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(
			Entry.DiffModel.DiffColor));
	}
	return FSlateColor(FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(
		SourceColor.GetSpecifiedColor()));
}

EVisibility FBlueprintHelperReviewRowHighlightModel::ResolveRowActionsVisibility(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText)
{
	FRowHighlightEntry Entry;
	return FindExactRowHighlightEntry(AssetPath, Surface, SearchText, Entry) && Entry.bSelected
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
					return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(
						AssetPath,
						Surface,
						SearchText,
						EBlueprintHelperReviewActionIntentKind::Accept,
						TEXT("row_highlight_overlay"));
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Reject")))
				.OnClicked_Lambda([AssetPath, Surface, SearchText]()
				{
					return FBlueprintHelperReviewRowHighlightModel::DispatchRowAction(
						AssetPath,
						Surface,
						SearchText,
						EBlueprintHelperReviewActionIntentKind::Reject,
						TEXT("row_highlight_overlay"));
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
		|| Surface == EBlueprintHelperReviewSurface::DataAsset
		|| Surface == EBlueprintHelperReviewSurface::Material;
}

void FBlueprintHelperReviewRowHighlightModel::InvalidateSurfaceState(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface)
{
	if (AssetPath.IsEmpty())
	{
		return;
	}
	if (GetRowHighlightSurfaceStates().Remove(BuildRowHighlightStateKey(AssetPath, Surface)) > 0)
	{
		BroadcastStateChanged(AssetPath, Surface, NextStateRevision());
	}
}

void FBlueprintHelperReviewRowHighlightModel::InvalidateAssetStates(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return;
	}

	TArray<TPair<FString, EBlueprintHelperReviewSurface>> StatesToRemove;
	for (const TPair<FString, FRowHighlightSurfaceState>& Pair : GetRowHighlightSurfaceStates())
	{
		if (Pair.Value.AssetPath == AssetPath || Pair.Key.StartsWith(AssetPath + TEXT("|")))
		{
			StatesToRemove.Add(TPair<FString, EBlueprintHelperReviewSurface>(
				Pair.Value.AssetPath,
				Pair.Value.Surface));
		}
	}
	for (const TPair<FString, EBlueprintHelperReviewSurface>& StateToRemove : StatesToRemove)
	{
		if (GetRowHighlightSurfaceStates().Remove(BuildRowHighlightStateKey(StateToRemove.Key, StateToRemove.Value)) > 0)
		{
			BroadcastStateChanged(StateToRemove.Key, StateToRemove.Value, NextStateRevision());
		}
	}
}

FLinearColor FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(const FLinearColor& ChangeColor)
{
	FLinearColor FillColor = ChangeColor == FLinearColor::Transparent
		? FLinearColor::Yellow
		: ChangeColor;
	FillColor.A = 0.6f;
	return FillColor;
}

void FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	EBlueprintHelperReviewSurface Surface,
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&),
	const FString& PreferredAssetPath,
	bool bEmitGeometryDiagnostics)
{
	if (!IsRowHighlightSurface(Surface) || !Args.ChangeItems || !Args.SurfaceDiffModels)
	{
		return;
	}

	const FString ContextAssetPath = Args.AssetContext ? Args.AssetContext->AssetPath : FString();
	const FString SelectedAssetPath = Args.SelectedChange.IsValid() ? Args.SelectedChange->AssetPath : FString();
	TArray<FString> AssetPaths;
	AddStateAssetPath(PreferredAssetPath, AssetPaths);
	AddStateAssetPath(ContextAssetPath, AssetPaths);
	AddStateAssetPath(SelectedAssetPath, AssetPaths);

	TSet<FString> DesiredStateKeys;
	const auto FindChangeById = [&Args](const FString& ReviewEventId)
	{
		for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Candidate : *Args.ChangeItems)
		{
			if (Candidate.IsValid() && Candidate->ChangeId == ReviewEventId)
			{
				return Candidate;
			}
		}
		return TSharedPtr<FBlueprintHelperReviewVisibleChange>();
	};

	for (const FString& AssetPath : AssetPaths)
	{
		const FString StateKey = BuildRowHighlightStateKey(AssetPath, Surface);
		DesiredStateKeys.Add(StateKey);

		FRowHighlightSurfaceState State;
		State.AssetPath = AssetPath;
		State.Surface = Surface;
		State.OnReviewActionIntent = Args.OnReviewActionIntent;
		State.GetChangeColor = Args.GetChangeColor;

		TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> HighlightedItems;
		TMap<FString, FString> PrimaryTargetByChangeId;
		for (const FBlueprintHelperReviewSurfaceDiffProjectionModel& DiffModel : *Args.SurfaceDiffModels)
		{
			TSharedPtr<FBlueprintHelperReviewVisibleChange> Item = FindChangeById(DiffModel.ReviewEventId);
			const bool bUsesContextAssetScope = !ContextAssetPath.IsEmpty() && AssetPath == ContextAssetPath;
			if (!Item.IsValid() || (!bUsesContextAssetScope && Item->AssetPath != AssetPath))
			{
				continue;
			}

			if (bEmitGeometryDiagnostics && Args.AddDebugMessage)
			{
				EmitDedupedRowHighlightDebug(
					Args.AddDebugMessage,
					FString::Printf(
						TEXT("ReviewRoute change=%s surface=%s result=shown reason=surface_diff_model target=\"%s\""),
						*Item->ChangeId,
						SurfaceDebugName(Surface),
						*DiffModel.TargetKey),
					Surface,
					Item->ChangeId,
					TEXT("shown"),
					TEXT("surface_diff_model"));
			}
			if (Predicate && !Predicate(*Item))
			{
				continue;
			}

			TArray<FString> Keys;
			CollectProjectionModelKeys(DiffModel, Keys);
			if (Keys.Num() == 0)
			{
				continue;
			}

			FString PrimaryTarget = DiffModel.TargetKey;
			if (PrimaryTarget.IsEmpty())
			{
				PrimaryTarget = DiffModel.DisplayLabel;
			}
			if (PrimaryTarget.IsEmpty())
			{
				PrimaryTarget = Keys[0];
			}
			if (!PrimaryTargetByChangeId.Contains(Item->ChangeId))
			{
				PrimaryTargetByChangeId.Add(Item->ChangeId, PrimaryTarget);
			}

			for (const FString& Key : Keys)
			{
				FRowHighlightEntry Entry;
				Entry.ChangeId = Item->ChangeId;
				Entry.TargetKey = Key;
				Entry.ChangeKind = DiffModel.ChangeKind;
				Entry.bSelected = IsSameChange(Item, Args.SelectedChange);
				Entry.Change = Item;
				Entry.Binding = FBlueprintHelperReviewPanelStateService::MakeChangeBinding(*Item, Surface, Key);
				Entry.DiffModel = DiffModel;
				State.TargetKeyToHighlight.Add(Key, Entry);
			}
			HighlightedItems.Add(Item);
		}

		if (FRowHighlightSurfaceState* ExistingState = GetRowHighlightSurfaceStates().Find(StateKey))
		{
			if (AreSurfaceStatesEquivalent(*ExistingState, State))
			{
				const uint64 ExistingRevision = ExistingState->Revision;
				*ExistingState = MoveTemp(State);
				ExistingState->Revision = ExistingRevision;
			}
			else
			{
				State.Revision = NextStateRevision();
				GetRowHighlightSurfaceStates().Add(StateKey, State);
				BroadcastStateChanged(AssetPath, Surface, State.Revision);
			}
		}
		else
		{
			State.Revision = NextStateRevision();
			GetRowHighlightSurfaceStates().Add(StateKey, State);
			BroadcastStateChanged(AssetPath, Surface, State.Revision);
		}

		const bool bShouldEmitGeometryDiagnostics =
			bEmitGeometryDiagnostics
			&& (AssetPath == ContextAssetPath
				|| AssetPath == SelectedAssetPath
				|| AssetPath == PreferredAssetPath);
		if (!bShouldEmitGeometryDiagnostics)
		{
			continue;
		}

		for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : HighlightedItems)
		{
			FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;
			const bool bResolved = Args.ResolveRowGeometry.IsBound()
				&& Args.ResolveRowGeometry.Execute(*Item, Surface, Anchor)
				&& Anchor.bIsValid;
			const FString TargetText = PrimaryTargetByChangeId.FindRef(Item->ChangeId);

			if (bResolved)
			{
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
	}

	TArray<TPair<FString, EBlueprintHelperReviewSurface>> StaleStates;
	for (const TPair<FString, FRowHighlightSurfaceState>& Pair : GetRowHighlightSurfaceStates())
	{
		if (Pair.Value.Surface == Surface && !DesiredStateKeys.Contains(Pair.Key))
		{
			StaleStates.Add(TPair<FString, EBlueprintHelperReviewSurface>(
				Pair.Value.AssetPath,
				Pair.Value.Surface));
		}
	}
	for (const TPair<FString, EBlueprintHelperReviewSurface>& StaleState : StaleStates)
	{
		if (GetRowHighlightSurfaceStates().Remove(BuildRowHighlightStateKey(StaleState.Key, StaleState.Value)) > 0)
		{
			BroadcastStateChanged(StaleState.Key, StaleState.Value, NextStateRevision());
		}
	}
}

TSharedRef<SWidget> FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	EBlueprintHelperReviewSurface Surface,
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	RebuildSurfaceState(
		Args,
		Surface,
		Predicate,
		Args.AssetContext ? Args.AssetContext->AssetPath : FString(),
		true);
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


#include "UI/Review/BlueprintHelperReviewGeometrySearchService.h"
