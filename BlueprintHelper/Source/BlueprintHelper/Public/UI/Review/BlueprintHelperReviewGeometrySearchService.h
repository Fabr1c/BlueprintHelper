// BlueprintHelper Review geometry search service.

#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewGeometrySearchService
{
public:
	static FString NormalizeSearchText(FString Text);
	static void AddUniqueSearchCandidate(TArray<FString>& OutCandidates, FString Candidate);
	static void AddDisplaySearchCandidatesFromText(const FString& RawText, TArray<FString>& OutCandidates);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool DisplaySearchTextMatches(const FString& RowText, const FString& TargetText);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
};
