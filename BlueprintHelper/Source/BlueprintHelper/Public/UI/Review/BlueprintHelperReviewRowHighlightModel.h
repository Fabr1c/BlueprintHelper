// BlueprintHelper Review row highlight model.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Styling/SlateColor.h"
#include "Templates/Function.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewSurfaceDiffModel.h"

class SCanvas;
struct FBlueprintHelperReviewPanelSurfacePresenterArgs;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FBlueprintHelperReviewRowHighlightStateChanged,
	const FString&,
	EBlueprintHelperReviewSurface,
	uint64);

/**
 * 琛岄珮浜ā鍨嬶紝鎻愪緵琛岄珮浜浉鍏崇殑闈欐€佸伐鍏锋柟娉曘€?
 */
class BLUEPRINTHELPER_API FBlueprintHelperReviewRowHighlightModel
{
public:
	static FDelegateHandle AddStateChangedHandler(
		FBlueprintHelperReviewRowHighlightStateChanged::FDelegate InDelegate);
	static void RemoveStateChangedHandler(FDelegateHandle InHandle);

	static bool IsRowHighlightSurface(EBlueprintHelperReviewSurface Surface);

	static FLinearColor GetRowHighlightFillColor(const FLinearColor& ChangeColor);

	static TSharedRef<SWidget> BuildRowHighlightOverlay(
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface,
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));

	static void RebuildSurfaceState(
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface,
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&),
		const FString& PreferredAssetPath = FString(),
		bool bEmitGeometryDiagnostics = false);

	static void InvalidateSurfaceState(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface);

	static void InvalidateAssetStates(const FString& AssetPath);

	static FSlateColor GetRowBackgroundColor(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

	static EVisibility GetRowActionsVisibility(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

	static bool TryGetRowActionBinding(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		FBlueprintHelperReviewRowBinding& OutBinding);

	static FReply DispatchRowAction(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		EBlueprintHelperReviewActionIntentKind Action,
		const FString& SourceWidget);

private:
	struct FRowHighlightEntry : public FBlueprintHelperReviewRowHighlight
	{
		TSharedPtr<FBlueprintHelperReviewVisibleChange> Change;
		FBlueprintHelperReviewRowBinding Binding;
		FBlueprintHelperReviewSurfaceDiffProjectionModel DiffModel;
	};

	struct FRowHighlightSurfaceState
	{
		FString AssetPath;
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
		uint64 Revision = 0;
		TMap<FString, FRowHighlightEntry> TargetKeyToHighlight;
		TFunction<FReply(const FBlueprintHelperReviewActionIntent&)> OnReviewActionIntent;
		TFunction<FSlateColor(EBlueprintHelperReviewChangeKind)> GetChangeColor;
	};

	static TMap<FString, FRowHighlightSurfaceState>& GetRowHighlightSurfaceStates();
	static TSet<FString>& GetEmittedRowHighlightDebugKeys();
	static FBlueprintHelperReviewRowHighlightStateChanged& GetStateChangedDelegate();
	static uint64& GetStateRevisionCounter();
	static uint64 NextStateRevision();
	static void BroadcastStateChanged(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		uint64 Revision);
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
	static const TCHAR* SurfaceDebugName(EBlueprintHelperReviewSurface Surface);
	static bool IsSameChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right);
	static bool AreBindingsEquivalent(
		const FBlueprintHelperReviewRowBinding& Left,
		const FBlueprintHelperReviewRowBinding& Right);
	static bool AreEntriesEquivalent(
		const FRowHighlightEntry& Left,
		const FRowHighlightEntry& Right);
	static bool AreSurfaceStatesEquivalent(
		const FRowHighlightSurfaceState& Left,
		const FRowHighlightSurfaceState& Right);
	static FString BuildRowHighlightStateKey(const FString& AssetPath, EBlueprintHelperReviewSurface Surface);
	static void AddStateAssetPath(const FString& AssetPath, TArray<FString>& OutAssetPaths);
	static FString ExtractReadableTail(FString Text);
	static void AddRowHighlightKey(const FString& Key, TArray<FString>& OutKeys);
	static void CollectProjectionModelKeys(
		const FBlueprintHelperReviewSurfaceDiffProjectionModel& DiffModel,
		TArray<FString>& OutKeys);
	static FString GetReviewListTargetText(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		EBlueprintHelperReviewSurface Surface);
	static void CollectRowHighlightKeys(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		TArray<FString>& OutKeys);
	static bool FindRowHighlightEntry(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		FRowHighlightEntry& OutEntry,
		bool bAllowFuzzyMatch);
	static bool FindExactRowHighlightEntry(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		FRowHighlightEntry& OutEntry);
	static FSlateColor ResolveRowHighlightColor(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);
	static EVisibility ResolveRowActionsVisibility(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);
	static TSharedRef<SWidget> BuildComponentRowHighlightFill(const FSlateColor& FillColor);
	static TSharedRef<SWidget> BuildComponentRowActions(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);
	static void AddComponentRowOverlay(
		const TSharedPtr<SCanvas>& Canvas,
		const FBlueprintHelperReviewSurfaceGeometryAnchor& Anchor,
		const FSlateColor& FillColor,
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		bool bSelected);
	static void EmitDedupedRowHighlightDebug(
		const TFunction<void(const FString&)>& AddDebugMessage,
		const FString& Message,
		EBlueprintHelperReviewSurface Surface,
		const FString& ChangeId,
		const FString& Result,
		const FString& Reason);
};

