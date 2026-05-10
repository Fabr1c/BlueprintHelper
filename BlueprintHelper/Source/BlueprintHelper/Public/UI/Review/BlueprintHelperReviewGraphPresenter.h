// BlueprintHelper Review graph presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SWidget.h"

class SGraphEditor;
class UBlueprint;
class UEdGraph;
struct FBlueprintHelperReviewAssetContext;

class BLUEPRINTHELPER_API FBlueprintHelperReviewGraphPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewGraphPresenterArgs& Args,
		FBlueprintHelperReviewGraphPresenterState& State);
	static UEdGraph* ResolveGraphForSelection(
		const FBlueprintHelperReviewAssetContext& Context,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange);

private:
	static TSharedRef<SWidget> BuildReviewPlaceholder(const FString& Message);
	static UBlueprint* CreateReviewPreviewBlueprint(const UBlueprint* SourceBlueprint);
	static void AttachPreviewGraphToMatchingBlueprintList(
		const UBlueprint* SourceBlueprint,
		const UEdGraph* SourceGraph,
		UBlueprint* PreviewBlueprint,
		UEdGraph* PreviewGraph);
	static bool IsSameChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right);
	static bool BuildGraphBoundsForChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const UEdGraph* PreviewGraphToEdit,
		const FString& GraphName,
		const TSharedPtr<SGraphEditor>& GraphEditorForBounds,
		FVector2D& OutPosition,
		FVector2D& OutSize,
		FString* OutDebugSummary);
	static void AddGraphDiffBlocks(
		UEdGraph* PreviewGraphToEdit,
		const UEdGraph* SourceGraph,
		const TSharedPtr<SGraphEditor>& GraphEditorForBounds,
		const FBlueprintHelperReviewGraphPresenterArgs& Args);
	static void JumpToSelectedGraphDiffBlock(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
		FBlueprintHelperReviewGraphPresenterState& State,
		const TFunction<void(const FString&)>& AddDebugMessage);
};
