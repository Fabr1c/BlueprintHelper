// BlueprintHelper Review surface content presenter shared data types.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class SWidget;

enum class EBlueprintHelperReviewSurfaceContentHost : uint8
{
	Unknown,
	MainWorkspace,
	StructurePanel
};

enum class EBlueprintHelperReviewSurfaceHostSlot : uint8
{
	Structure,
	MyBlueprint,
	Details,
	MainWorkspace
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceHostBinding
{
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	EBlueprintHelperReviewSurfaceHostSlot HostSlot = EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace;
	bool bSupportsOverlayRefresh = true;
	bool bSupportsRowRefresh = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPanelSurfaceContentArgs
	: public FBlueprintHelperReviewPanelSurfacePresenterArgs
{
	EBlueprintHelperReviewSurfaceContentHost Host =
		EBlueprintHelperReviewSurfaceContentHost::Unknown;

	FBlueprintHelperReviewGraphPresenterState* GraphState = nullptr;
	FBlueprintHelperReviewBlueprintComponentsPresenter::FState* ComponentsState = nullptr;
	FBlueprintHelperReviewWidgetTreePresenterState* WidgetTreeState = nullptr;
	FBlueprintHelperReviewMyBlueprintPresenter::FState* MyBlueprintState = nullptr;
	FBlueprintHelperReviewDataTablePresenterState* DataTableState = nullptr;
	FBlueprintHelperReviewDataAssetPresenterState* DataAssetState = nullptr;
	FBlueprintHelperReviewMaterialPresenterState* MaterialState = nullptr;

	FString RequestedGraphNavigationChangeId;
	FString RequestedGraphNavigationGraphName;
	bool bAllowGraphNavigationWithoutGraphReview = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceRowsRefreshArgs
	: public FBlueprintHelperReviewPanelSurfaceContentArgs
{
	TMap<EBlueprintHelperReviewSurfaceHostSlot, TWeakPtr<SWidget>> HostWidgets;
	TFunction<bool()> RefreshDetailsRows;
};
