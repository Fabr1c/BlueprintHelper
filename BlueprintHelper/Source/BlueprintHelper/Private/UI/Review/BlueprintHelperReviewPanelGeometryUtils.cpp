// BlueprintHelper Review panel geometry helpers.

#include "UI/Review/BlueprintHelperReviewPanelGeometryUtils.h"

#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Runtime/Launch/Resources/Version.h"

FString FBlueprintHelperReviewPanelGeometryUtils::NormalizeGeometrySearchText(FString Text)
{
	Text.ToLowerInline();
	for (int32 Index = Text.Len() - 1; Index >= 0; --Index)
	{
		if (!FChar::IsAlnum(Text[Index]))
		{
			Text.RemoveAt(Index);
		}
	}
	return Text;
}

void FBlueprintHelperReviewPanelGeometryUtils::AddUniqueSearchCandidate(
	TArray<FString>& OutCandidates,
	FString Candidate)
{
	Candidate.TrimStartAndEndInline();
	if (!Candidate.IsEmpty())
	{
		OutCandidates.AddUnique(Candidate);
	}
}

void FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(
	const FString& RawText,
	TArray<FString>& OutCandidates)
{
	FString Text = RawText;
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return;
	}

	AddUniqueSearchCandidate(OutCandidates, Text);

	int32 DelimiterIndex = INDEX_NONE;
	if (Text.FindLastChar(TEXT(':'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('/'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('.'), DelimiterIndex))
	{
		AddUniqueSearchCandidate(OutCandidates, Text.Mid(DelimiterIndex + 1));
	}
}

bool FBlueprintHelperReviewPanelGeometryUtils::SearchTextMatches(
	const FString& RowText,
	const FString& TargetText)
{
	const FString NormalizedRow = NormalizeGeometrySearchText(RowText);
	if (NormalizedRow.Len() < 2)
	{
		return false;
	}

	TArray<FString> Candidates;
	AddSearchCandidatesFromText(TargetText, Candidates);
	for (const FString& Candidate : Candidates)
	{
		const FString NormalizedCandidate = NormalizeGeometrySearchText(Candidate);
		if (NormalizedCandidate.Len() >= 2
			&& (NormalizedRow.Contains(NormalizedCandidate)
				|| NormalizedCandidate.Contains(NormalizedRow)))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewPanelGeometryUtils::TryReadWidgetText(
	const TSharedRef<SWidget>& Widget,
	FString& OutText)
{
	const FString WidgetType = Widget->GetTypeAsString();
	if (WidgetType == TEXT("STextBlock"))
	{
		OutText = static_cast<STextBlock&>(Widget.Get()).GetText().ToString();
		return true;
	}
	if (WidgetType == TEXT("SRichTextBlock"))
	{
		OutText = static_cast<SRichTextBlock&>(Widget.Get()).GetText().ToString();
		return true;
	}
	if (WidgetType == TEXT("SInlineEditableTextBlock"))
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		OutText = static_cast<SInlineEditableTextBlock&>(Widget.Get()).GetText().ToString();
		return true;
#else
		OutText = Widget->GetAccessibleText().ToString();
		return !OutText.IsEmpty();
#endif
	}
	if (WidgetType == TEXT("SEditableText"))
	{
		OutText = static_cast<SEditableText&>(Widget.Get()).GetText().ToString();
		return true;
	}
	if (WidgetType == TEXT("SEditableTextBox"))
	{
		OutText = static_cast<SEditableTextBox&>(Widget.Get()).GetText().ToString();
		return true;
	}
	return false;
}

bool FBlueprintHelperReviewPanelGeometryUtils::BuildGeometryAnchorFromWidget(
	const TSharedRef<SWidget>& SourceWidget,
	const TSharedPtr<SWidget>& OverlayWidget,
	const FString& TargetText,
	const TCHAR* DebugMode,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	const FGeometry SourceGeometry = SourceWidget->GetCachedGeometry();
	const FGeometry HostGeometry = OverlayWidget->GetCachedGeometry();
	const FVector2D SourceSize = SourceGeometry.GetLocalSize();
	const FVector2D HostSize = HostGeometry.GetLocalSize();
	if (SourceSize.X <= 0.0f || SourceSize.Y <= 0.0f || HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
	{
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("slate_text_geometry_not_ready");
		return false;
	}

	const FVector2D LocalTopLeft = HostGeometry.AbsoluteToLocal(SourceGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	OutAnchor.bIsValid = true;
	OutAnchor.Position = FVector2D(0.0f, FMath::Max(0.0f, LocalTopLeft.Y - 4.0f));
	OutAnchor.Size = FVector2D(HostSize.X, FMath::Max(SourceSize.Y + 8.0f, 20.0f));
	OutAnchor.HostSize = HostSize;
	OutAnchor.TargetText = TargetText;
	OutAnchor.Reason = TEXT("stable_slate_text_geometry");
	OutAnchor.DebugMode = DebugMode ? DebugMode : TEXT("slate_text");
	return true;
}

bool FBlueprintHelperReviewPanelGeometryUtils::ResolveTextGeometryRecursive(
	const TSharedRef<SWidget>& Widget,
	const TSharedPtr<SWidget>& OverlayWidget,
	const TArray<FString>& Candidates,
	const TCHAR* DebugMode,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!Widget->GetVisibility().IsVisible())
	{
		return false;
	}

	FString WidgetText;
	if (TryReadWidgetText(Widget, WidgetText))
	{
		for (const FString& Candidate : Candidates)
		{
			if (SearchTextMatches(WidgetText, Candidate))
			{
				return BuildGeometryAnchorFromWidget(Widget, OverlayWidget, Candidate, DebugMode, OutAnchor);
			}
		}
	}

	FChildren* Children = Widget->GetChildren();
	if (!Children)
	{
		return false;
	}
	for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		if (ResolveTextGeometryRecursive(
			Children->GetChildAt(ChildIndex),
			OverlayWidget,
			Candidates,
			DebugMode,
			OutAnchor))
		{
			return true;
		}
	}
	return false;
}
