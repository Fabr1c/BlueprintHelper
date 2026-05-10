// BlueprintHelper Review blueprint components presenter.

#include "UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "SSubobjectBlueprintEditor.h"
#include "SSubobjectEditor.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"

bool FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Components);
}

TSharedRef<SWidget> FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.SubobjectEditor.Reset();
	State.OnGeometryInvalidated = OnGeometryInvalidated;

	if (UBlueprint* Blueprint = Context.Blueprint.Get())
	{
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			if (AActor* ActorCDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
			{
				TSharedRef<SSubobjectBlueprintEditor> Editor = SAssignNew(State.SubobjectEditor, SSubobjectBlueprintEditor)
					.ObjectContext(ActorCDO)
					.AllowEditing(false)
					.HideComponentClassCombo(true);
				return SNew(SBlueprintHelperReviewGeometryProbe)
					.Surface(EBlueprintHelperReviewSurface::Components)
					.TargetKey(Context.AssetPath)
					.OnGeometryInvalidated(OnGeometryInvalidated)
					[
						Editor
					];
			}
		}
	}

	return BuildReviewPlaceholder(TEXT("No Blueprint component tree loaded."));
}

TSharedRef<SWidget> FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::Components,
		&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewBlueprintComponentsPresenter::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	FState& State,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!State.SubobjectEditor.IsValid())
	{
		OutAnchor.Reason = TEXT("component_editor_unavailable");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<FString> Candidates;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface != EBlueprintHelperReviewSurface::Components)
		{
			continue;
		}
		AddComponentNameCandidatesFromText(Target.ComponentPath, Candidates);
		AddComponentNameCandidatesFromText(Target.TargetKey, Candidates);
		AddComponentNameCandidatesFromText(Target.PropertyPath, Candidates);
		AddComponentNameCandidatesFromText(Target.DisplayLabel, Candidates);
	}
	AddComponentNameCandidatesFromText(Change.LocationKey, Candidates);
	AddComponentNameCandidatesFromText(Change.DisplayLabel, Candidates);

	if (Candidates.Num() == 0)
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const TSharedPtr<SSubobjectEditorDragDropTree> Tree = State.SubobjectEditor->GetDragDropTree();
	if (!Tree.IsValid())
	{
		OutAnchor.Reason = TEXT("component_tree_unavailable");
		return false;
	}

	bool bFoundNode = false;
	for (const FString& Candidate : Candidates)
	{
		FSubobjectEditorTreeNodePtrType Node = State.SubobjectEditor->FindSlateNodeForVariableName(FName(*Candidate));
		if (!Node.IsValid())
		{
			TArray<FSubobjectEditorTreeNodePtrType> PendingNodes = State.SubobjectEditor->GetRootNodes();
			for (int32 NodeIndex = 0; NodeIndex < PendingNodes.Num(); ++NodeIndex)
			{
				const FSubobjectEditorTreeNodePtrType& PendingNode = PendingNodes[NodeIndex];
				if (!PendingNode.IsValid())
				{
					continue;
				}

				if (SearchTextMatchesAnyCandidate(PendingNode->GetVariableName().ToString(), Candidates)
					|| SearchTextMatchesAnyCandidate(PendingNode->GetDisplayString(), Candidates))
				{
					Node = PendingNode;
					break;
				}

				PendingNodes.Append(PendingNode->GetChildren());
			}
		}
		if (!Node.IsValid())
		{
			continue;
		}

		bFoundNode = true;
		const TSharedPtr<ITableRow> Row = Tree->WidgetFromItem(Node);
		if (!Row.IsValid())
		{
			Tree->RequestScrollIntoView(Node);
			OutAnchor.TargetText = Candidate;
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			return false;
		}

		const FString AssetPath = Change.AssetPath;
		const FSlateColor RowColor = FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			AssetPath,
			EBlueprintHelperReviewSurface::Components,
			Candidate);
		TryApplyTableRowBackgroundColor(Row->AsWidget(), RowColor);

		return BuildGeometryAnchorFromRowWidget(
			Row->AsWidget(),
			OverlayWidget,
			Candidate,
			TEXT("subobject_row"),
			OutAnchor);
	}

	OutAnchor.Reason = bFoundNode ? TEXT("slate_row_geometry_not_ready") : TEXT("no_matching_component_row");
	return false;
}

TSharedRef<SWidget> FBlueprintHelperReviewBlueprintComponentsPresenter::BuildReviewPlaceholder(const FString& Message)
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

bool FBlueprintHelperReviewBlueprintComponentsPresenter::BuildGeometryAnchorFromRowWidget(
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

void FBlueprintHelperReviewBlueprintComponentsPresenter::AddUniqueCandidate(TArray<FString>& OutCandidates, FString Candidate)
{
	Candidate.TrimStartAndEndInline();
	if (!Candidate.IsEmpty())
	{
		OutCandidates.AddUnique(Candidate);
	}
}

void FBlueprintHelperReviewBlueprintComponentsPresenter::AddComponentNameCandidatesFromText(
	const FString& RawText,
	TArray<FString>& OutCandidates)
{
	FString Text = RawText;
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return;
	}

	int32 DelimiterIndex = INDEX_NONE;
	if (Text.FindLastChar(TEXT(':'), DelimiterIndex))
	{
		Text = Text.Mid(DelimiterIndex + 1);
	}
	if (Text.FindLastChar(TEXT('/'), DelimiterIndex))
	{
		Text = Text.Mid(DelimiterIndex + 1);
	}

	FString ComponentPart = Text;
	if (ComponentPart.FindChar(TEXT('.'), DelimiterIndex))
	{
		ComponentPart.LeftInline(DelimiterIndex);
	}
	AddUniqueCandidate(OutCandidates, ComponentPart);
	AddUniqueCandidate(OutCandidates, Text);
}

FString FBlueprintHelperReviewBlueprintComponentsPresenter::NormalizeGeometrySearchText(FString Text)
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

void FBlueprintHelperReviewBlueprintComponentsPresenter::AddGeometrySearchTerms(
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

bool FBlueprintHelperReviewBlueprintComponentsPresenter::GeometrySearchTextMatches(
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

bool FBlueprintHelperReviewBlueprintComponentsPresenter::SearchTextMatchesAnyCandidate(
	const FString& SearchText,
	const TArray<FString>& Candidates)
{
	for (const FString& Candidate : Candidates)
	{
		if (GeometrySearchTextMatches(SearchText, Candidate))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewBlueprintComponentsPresenter::TryApplyTableRowBackgroundColor(
	const TSharedRef<SWidget>& RowWidget,
	const FSlateColor& RowColor)
{
	const FLinearColor SpecifiedColor = RowColor.GetSpecifiedColor();
	if (SpecifiedColor.A <= 0.0f)
	{
		return false;
	}

	const FString RowType = RowWidget->GetTypeAsString();
	if (!RowType.Contains(TEXT("TableRow")) && !RowType.Contains(TEXT("Subobject_RowWidget")))
	{
		return false;
	}

	SBorder* BorderRow = static_cast<SBorder*>(&RowWidget.Get());
	BorderRow->SetBorderBackgroundColor(RowColor);
	return true;
}
