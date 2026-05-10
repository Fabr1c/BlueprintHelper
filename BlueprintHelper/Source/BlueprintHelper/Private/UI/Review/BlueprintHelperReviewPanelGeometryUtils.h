// BlueprintHelper Review panel geometry helpers.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class SWidget;

class FBlueprintHelperReviewPanelGeometryUtils
{
public:
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddUniqueSearchCandidate(TArray<FString>& OutCandidates, FString Candidate);
	static void AddSearchCandidatesFromText(const FString& RawText, TArray<FString>& OutCandidates);
	static bool SearchTextMatches(const FString& RowText, const FString& TargetText);
	static bool TryReadWidgetText(const TSharedRef<SWidget>& Widget, FString& OutText);
	static bool BuildGeometryAnchorFromWidget(
		const TSharedRef<SWidget>& SourceWidget,
		const TSharedPtr<SWidget>& OverlayWidget,
		const FString& TargetText,
		const TCHAR* DebugMode,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
	static bool ResolveTextGeometryRecursive(
		const TSharedRef<SWidget>& Widget,
		const TSharedPtr<SWidget>& OverlayWidget,
		const TArray<FString>& Candidates,
		const TCHAR* DebugMode,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};
