// BlueprintHelper ReviewPanel settings resolver implementation.

#include "UI/Review/BlueprintHelperReviewPanelSettingsResolver.h"

#include "Dom/JsonValue.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

namespace
{
static TArray<float> BlueprintHelperReviewResolveFloatArray(
	const FString& DotPath,
	const TArray<float>& DefaultValue)
{
	const TSharedPtr<FJsonValue> JsonValue = FBlueprintHelperRuntimeSettingResolver::GetJsonValue(DotPath);
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!JsonValue.IsValid() || !JsonValue->TryGetArray(JsonArray) || !JsonArray)
	{
		return DefaultValue;
	}

	TArray<float> Result;
	Result.Reserve(JsonArray->Num());
	for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
	{
		double Number = 0.0;
		if (!Entry.IsValid() || !Entry->TryGetNumber(Number))
		{
			return DefaultValue;
		}
		Result.Add(FMath::Max(0.0f, static_cast<float>(Number)));
	}

	return Result.Num() == DefaultValue.Num() ? Result : DefaultValue;
}

static float BlueprintHelperReviewResolveNonNegativeFloat(
	const FString& DotPath,
	const float DefaultValue)
{
	return FMath::Max(
		0.0f,
		static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(DotPath, DefaultValue)));
}

static float BlueprintHelperReviewResolveUnitFloat(
	const FString& DotPath,
	const float DefaultValue)
{
	return FMath::Clamp(
		static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(DotPath, DefaultValue)),
		0.0f,
		1.0f);
}
}

FBlueprintHelperReviewPanelSettings FBlueprintHelperReviewPanelSettingsResolver::Load()
{
	FBlueprintHelperReviewPanelSettings Settings;

	Settings.MainSplitRatio = BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.main_split_ratio"),
		Settings.MainSplitRatio);
	Settings.ComponentBlueprintSplit = BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.component_blueprint_split"),
		Settings.ComponentBlueprintSplit);
	Settings.MainGraphRatio = BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.main_graph_ratio"),
		Settings.MainGraphRatio);
	Settings.RightBottomRatio = BlueprintHelperReviewResolveFloatArray(
		TEXT("ui.review_panel.right_bottom_ratio"),
		Settings.RightBottomRatio);

	const FVector2D RootRowPadding = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.review_panel.root_row_padding"),
		FVector2D(Settings.RootRowPadding.Left, Settings.RootRowPadding.Top));
	Settings.RootRowPadding = FMargin(
		FMath::Max(0.0f, RootRowPadding.X),
		FMath::Max(0.0f, RootRowPadding.Y));

	Settings.RowContentPadding = BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.row_content_padding"),
		Settings.RowContentPadding);
	Settings.DiffFrameOuterPadding = BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.diff_frame_outer_padding"),
		Settings.DiffFrameOuterPadding);
	Settings.DiffActionPadding = BlueprintHelperReviewResolveNonNegativeFloat(
		TEXT("ui.review_panel.diff_action_padding"),
		Settings.DiffActionPadding);
	Settings.DiffActionSpacing = FBlueprintHelperRuntimeSettingResolver::GetMargin(
		TEXT("ui.review_panel.diff_action_spacing"),
		Settings.DiffActionSpacing);
	Settings.SurfaceOverlayFillAlpha = BlueprintHelperReviewResolveUnitFloat(
		TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		Settings.SurfaceOverlayFillAlpha);
	Settings.SurfaceOverlaySelectedFillAlpha = BlueprintHelperReviewResolveUnitFloat(
		TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		Settings.SurfaceOverlaySelectedFillAlpha);
	Settings.SurfaceGeometryPadding = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.review_panel.surface_geometry_padding"),
		Settings.SurfaceGeometryPadding);
	Settings.SurfaceGeometryPadding.X = FMath::Max(0.0f, Settings.SurfaceGeometryPadding.X);
	Settings.SurfaceGeometryPadding.Y = FMath::Max(0.0f, Settings.SurfaceGeometryPadding.Y);
	Settings.FlashTickDecay = BlueprintHelperReviewResolveNonNegativeFloat(
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
