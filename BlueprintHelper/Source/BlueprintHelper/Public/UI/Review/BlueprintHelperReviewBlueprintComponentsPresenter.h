// BlueprintHelper Review blueprint components presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SWidget.h"

class SSubobjectBlueprintEditor;
struct FBlueprintHelperReviewAssetContext;

class BLUEPRINTHELPER_API FBlueprintHelperReviewBlueprintComponentsPresenter
{
public:
	struct FState
	{
		TSharedPtr<SSubobjectBlueprintEditor> SubobjectEditor;
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	};

	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
	static bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		FState& State,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);

private:
	static TSharedRef<SWidget> BuildReviewPlaceholder(const FString& Message);
	static bool BuildGeometryAnchorFromRowWidget(
		const TSharedPtr<SWidget>& RowWidget,
		const TSharedPtr<SWidget>& OverlayWidget,
		const FString& TargetText,
		const TCHAR* DebugMode,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
	static void AddUniqueCandidate(TArray<FString>& OutCandidates, FString Candidate);
	static void AddComponentNameCandidatesFromText(const FString& RawText, TArray<FString>& OutCandidates);
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
	static bool SearchTextMatchesAnyCandidate(const FString& SearchText, const TArray<FString>& Candidates);
	static bool TryApplyTableRowBackgroundColor(
		const TSharedRef<SWidget>& RowWidget,
		const FSlateColor& RowColor);
};
