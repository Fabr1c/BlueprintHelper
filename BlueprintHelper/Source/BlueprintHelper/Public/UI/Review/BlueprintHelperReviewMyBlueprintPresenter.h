// BlueprintHelper Review MyBlueprint presenter.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewAssetContext;
struct FBlueprintHelperReviewPanelSurfacePresenterArgs;
template <typename ItemType> class STreeView;

DECLARE_DELEGATE_TwoParams(FBlueprintHelperReviewMyBlueprintNavigateToGraph, const FString&, const FString&);

class BLUEPRINTHELPER_API FBlueprintHelperReviewMyBlueprintPresenter
{
public:
	enum class ERowKind : uint8
	{
		Section,
		Graph,
		Function,
		Macro,
		Event,
		Dispatcher,
		Variable,
		ReviewOnly
	};

	struct FRowItem
	{
		FText Label;
		FString SearchText;
		FName IconName;
		FEdGraphPinType PinType;
		bool bHasPinType = false;
		ERowKind Kind = ERowKind::ReviewOnly;
		FString NavigateChangeId;
		FString NavigateGraphName;
		TArray<TSharedPtr<FRowItem>> Children;
		TWeakPtr<SWidget> RowWidget;
	};

	struct FState
	{
		TArray<TSharedPtr<FRowItem>> RootItems;
		TSharedPtr<STreeView<TSharedPtr<FRowItem>>> TreeView;
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	};

	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FState& State,
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated(),
		FBlueprintHelperReviewMyBlueprintNavigateToGraph OnNavigateToGraph = FBlueprintHelperReviewMyBlueprintNavigateToGraph());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
	static bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		FState& State,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};
