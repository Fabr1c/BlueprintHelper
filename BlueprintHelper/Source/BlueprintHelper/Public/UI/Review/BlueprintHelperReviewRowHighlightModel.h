// BlueprintHelper Review row highlight model.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;

/**
 * 行高亮模型，提供行高亮相关的静态工具方法。
 */
class BLUEPRINTHELPER_API FBlueprintHelperReviewRowHighlightModel
{
public:
	static bool IsRowHighlightSurface(EBlueprintHelperReviewSurface Surface);

	static FLinearColor GetRowHighlightFillColor(const FLinearColor& ChangeColor);

	static TMap<FString, FBlueprintHelperReviewRowHighlight> BuildTargetKeyToHighlight(
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
		EBlueprintHelperReviewSurface Surface,
		const FString& CurrentAssetPath);

	static TSharedRef<SWidget> BuildRowHighlightOverlay(
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface,
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));

	static FSlateColor GetRowBackgroundColor(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

	static EVisibility GetRowActionsVisibility(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

	static FReply AcceptHighlightedRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

	static FReply RejectHighlightedRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);

private:
	struct FRowHighlightEntry : public FBlueprintHelperReviewRowHighlight
	{
		TSharedPtr<FBlueprintHelperReviewVisibleChange> Change;
	};

	struct FRowHighlightSurfaceState
	{
		FString AssetPath;
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
		TMap<FString, FRowHighlightEntry> TargetKeyToHighlight;
		TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)> OnAcceptChange;
		TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)> OnRejectChange;
		TFunction<FSlateColor(EBlueprintHelperReviewChangeKind)> GetChangeColor;
	};

	static TMap<FString, FRowHighlightSurfaceState>& GetRowHighlightSurfaceStates();
	static TSet<FString>& GetEmittedRowHighlightDebugKeys();
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
	static const TCHAR* SurfaceDebugName(EBlueprintHelperReviewSurface Surface);
	static bool IsSameChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right);
	static FString BuildRowHighlightStateKey(const FString& AssetPath, EBlueprintHelperReviewSurface Surface);
	static FString ExtractReadableTail(FString Text);
	static void AddRowHighlightKey(const FString& Key, TArray<FString>& OutKeys);
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
		FRowHighlightEntry& OutEntry);
	static FReply ExecuteHighlightedRowAction(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		bool bAccept);
	static FSlateColor ResolveRowHighlightColor(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);
	static EVisibility ResolveRowActionsVisibility(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText);
	static void EmitDedupedRowHighlightDebug(
		const TFunction<void(const FString&)>& AddDebugMessage,
		const FString& Message,
		EBlueprintHelperReviewSurface Surface,
		const FString& ChangeId,
		const FString& Result,
		const FString& Reason);
};
