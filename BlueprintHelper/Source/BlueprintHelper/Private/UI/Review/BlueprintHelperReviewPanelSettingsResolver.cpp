// BlueprintHelper ReviewPanel settings resolver implementation.

#include "UI/Review/BlueprintHelperReviewPanelSettingsResolver.h"
#include "UI/Review/Utils/BlueprintHelperReviewUIUtils.h"

#include "Dom/JsonValue.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"


FBlueprintHelperReviewPanelSettings FBlueprintHelperReviewPanelSettingsResolver::Load()
{
	FBlueprintHelperReviewPanelSettings Settings;

	Settings.MainSplitRatio = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.main_split_ratio"),
		Settings.MainSplitRatio);
	Settings.ComponentBlueprintSplit = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.component_blueprint_split"),
		Settings.ComponentBlueprintSplit);
	Settings.MainGraphRatio = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.main_graph_ratio"),
		Settings.MainGraphRatio);
	Settings.RightBottomRatio = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.right_bottom_ratio"),
		Settings.RightBottomRatio);

	const FVector2D RootRowPadding = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.review_panel.root_row_padding"),
		FVector2D(Settings.RootRowPadding.Left, Settings.RootRowPadding.Top));
	Settings.RootRowPadding = FMargin(
		FMath::Max(0.0f, RootRowPadding.X),
		FMath::Max(0.0f, RootRowPadding.Y));

	Settings.RowContentPadding = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.row_content_padding"),
		Settings.RowContentPadding);
	Settings.DiffFrameOuterPadding = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.diff_frame_outer_padding"),
		Settings.DiffFrameOuterPadding);
	Settings.DiffActionPadding = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.diff_action_padding"),
		Settings.DiffActionPadding);
	Settings.DiffActionSpacing = FBlueprintHelperRuntimeSettingResolver::GetMargin(
		TEXT("ui.review_panel.diff_action_spacing"),
		Settings.DiffActionSpacing);
	Settings.SurfaceOverlayFillAlpha = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveUnitFloat(
		TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		Settings.SurfaceOverlayFillAlpha);
	Settings.SurfaceOverlaySelectedFillAlpha = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveUnitFloat(
		TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		Settings.SurfaceOverlaySelectedFillAlpha);
	Settings.SurfaceGeometryPadding = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.review_panel.surface_geometry_padding"),
		Settings.SurfaceGeometryPadding);
	Settings.SurfaceGeometryPadding.X = FMath::Max(0.0f, Settings.SurfaceGeometryPadding.X);
	Settings.SurfaceGeometryPadding.Y = FMath::Max(0.0f, Settings.SurfaceGeometryPadding.Y);
	Settings.FlashTickDecay = UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.flash_tick_decay"),
		Settings.FlashTickDecay);
	Settings.DebugMaxMessages = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("ui.review_panel.debug_max_messages"),
			Settings.DebugMaxMessages));
	Settings.bOverlayFilterCurrentAssetOnly = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.review_panel.overlay_filter_current_asset_only"),
		Settings.bOverlayFilterCurrentAssetOnly);

	return Settings;
}
