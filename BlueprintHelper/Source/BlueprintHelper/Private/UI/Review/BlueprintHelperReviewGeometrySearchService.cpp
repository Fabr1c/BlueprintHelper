// BlueprintHelper Review geometry search service.

#include "UI/Review/BlueprintHelperReviewGeometrySearchService.h"

FString FBlueprintHelperReviewGeometrySearchService::NormalizeSearchText(FString Text)
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

void FBlueprintHelperReviewGeometrySearchService::AddUniqueSearchCandidate(
	TArray<FString>& OutCandidates,
	FString Candidate)
{
	Candidate.TrimStartAndEndInline();
	if (!Candidate.IsEmpty())
	{
		OutCandidates.AddUnique(Candidate);
	}
}

void FBlueprintHelperReviewGeometrySearchService::AddDisplaySearchCandidatesFromText(
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

void FBlueprintHelperReviewGeometrySearchService::AddGeometrySearchTerms(
	const FString& RawText,
	TArray<FString>& OutTerms)
{
	OutTerms.AddUnique(NormalizeSearchText(RawText));
	FString CurrentPart;
	for (int32 Index = 0; Index < RawText.Len(); ++Index)
	{
		const TCHAR Character = RawText[Index];
		if (FChar::IsAlnum(Character))
		{
			CurrentPart.AppendChar(Character);
			continue;
		}

		const FString Term = NormalizeSearchText(CurrentPart);
		if (Term.Len() >= 2)
		{
			OutTerms.AddUnique(Term);
		}
		CurrentPart.Reset();
	}

	const FString TailTerm = NormalizeSearchText(CurrentPart);
	if (TailTerm.Len() >= 2)
	{
		OutTerms.AddUnique(TailTerm);
	}
}

bool FBlueprintHelperReviewGeometrySearchService::DisplaySearchTextMatches(
	const FString& RowText,
	const FString& TargetText)
{
	const FString NormalizedRow = NormalizeSearchText(RowText);
	if (NormalizedRow.Len() < 2)
	{
		return false;
	}

	TArray<FString> Candidates;
	AddDisplaySearchCandidatesFromText(TargetText, Candidates);
	for (const FString& Candidate : Candidates)
	{
		const FString NormalizedCandidate = NormalizeSearchText(Candidate);
		if (NormalizedCandidate.Len() >= 2
			&& (NormalizedRow.Contains(NormalizedCandidate)
				|| NormalizedCandidate.Contains(NormalizedRow)))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewGeometrySearchService::GeometrySearchTextMatches(
	const FString& RowSearchText,
	const FString& TargetText)
{
	const FString NormalizedRow = NormalizeSearchText(RowSearchText);
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
