// BlueprintHelper Review surface frame geometry utilities.

#include "UI/Review/BlueprintHelperReviewSurfaceFrameGeometryUtils.h"

namespace
{
FVector2D GBlueprintHelperReviewSurfaceFrameGeometryPadding(10.0f, 10.0f);
}

void BlueprintHelperReviewSetSurfaceFrameGeometryPadding(const FVector2D& Padding)
{
	GBlueprintHelperReviewSurfaceFrameGeometryPadding = FVector2D(
		FMath::Max(0.0f, Padding.X),
		FMath::Max(0.0f, Padding.Y));
}

void FBlueprintHelperReviewSurfaceFrameGeometryUtils::ApplyRowGeometryPadding(
	FBlueprintHelperReviewSurfaceGeometryAnchor& Anchor)
{
	const FVector2D Padding = GBlueprintHelperReviewSurfaceFrameGeometryPadding;
	FVector2D PaddedPosition = Anchor.Position - Padding;
	FVector2D PaddedSize = Anchor.Size + Padding * 2.0f;

	if (PaddedPosition.X < 0.0f)
	{
		PaddedSize.X += PaddedPosition.X;
		PaddedPosition.X = 0.0f;
	}
	if (PaddedPosition.Y < 0.0f)
	{
		PaddedSize.Y += PaddedPosition.Y;
		PaddedPosition.Y = 0.0f;
	}
	if (Anchor.HostSize.X > 0.0f && PaddedPosition.X + PaddedSize.X > Anchor.HostSize.X)
	{
		PaddedSize.X = Anchor.HostSize.X - PaddedPosition.X;
	}
	if (Anchor.HostSize.Y > 0.0f && PaddedPosition.Y + PaddedSize.Y > Anchor.HostSize.Y)
	{
		PaddedSize.Y = Anchor.HostSize.Y - PaddedPosition.Y;
	}

	Anchor.Position = PaddedPosition;
	Anchor.Size = FVector2D(FMath::Max(0.0f, PaddedSize.X), FMath::Max(0.0f, PaddedSize.Y));
}

bool FBlueprintHelperReviewSurfaceFrameGeometryUtils::TryResolveSlateRowGeometry(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetText,
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!Item.IsValid() || !Args.ResolveRowGeometry.IsBound())
	{
		OutAnchor.Reason = TEXT("no_stable_slate_geometry");
		return false;
	}

	FBlueprintHelperReviewSurfaceGeometryAnchor CandidateAnchor;
	const bool bResolved = Args.ResolveRowGeometry.Execute(*Item, Surface, CandidateAnchor);
	if (!bResolved || !CandidateAnchor.bIsValid)
	{
		OutAnchor = CandidateAnchor;
		if (OutAnchor.Reason.IsEmpty())
		{
			OutAnchor.Reason = TEXT("no_stable_slate_geometry");
		}
		return false;
	}

	if (CandidateAnchor.Size.X <= 0.0f || CandidateAnchor.Size.Y <= 0.0f)
	{
		OutAnchor = CandidateAnchor;
		OutAnchor.bIsValid = false;
		OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
		return false;
	}

	if (CandidateAnchor.TargetText.IsEmpty())
	{
		CandidateAnchor.TargetText = TargetText;
	}
	if (CandidateAnchor.Reason.IsEmpty())
	{
		CandidateAnchor.Reason = TEXT("stable_slate_row_geometry");
	}
	if (CandidateAnchor.DebugMode.IsEmpty())
	{
		CandidateAnchor.DebugMode = TEXT("slate_row");
	}

	OutAnchor = CandidateAnchor;
	return true;
}

FString FBlueprintHelperReviewSurfaceFrameGeometryUtils::NormalizeGeometrySearchText(FString Text)
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

void FBlueprintHelperReviewSurfaceFrameGeometryUtils::AddGeometrySearchTerms(
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

bool FBlueprintHelperReviewSurfaceFrameGeometryUtils::GeometrySearchTextMatches(
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
