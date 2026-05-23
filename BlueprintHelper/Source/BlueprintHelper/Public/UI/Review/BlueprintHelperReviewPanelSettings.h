// BlueprintHelper ReviewPanel runtime UI settings.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPanelSettings
{
	TArray<float> MainSplitRatio = { 0.14f, 0.86f };
	TArray<float> ComponentBlueprintSplit = { 0.42f, 0.58f };
	TArray<float> MainGraphRatio = { 0.62f, 0.20f };
	TArray<float> RightBottomRatio = { 0.76f, 0.24f };
	FMargin RootRowPadding = FMargin(7.0f, 5.0f);
	float RowContentPadding = 6.0f;
	float DiffFrameOuterPadding = 3.0f;
	float DiffActionPadding = 5.0f;
	FMargin DiffActionSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
	float SurfaceOverlayFillAlpha = 0.60f;
	float SurfaceOverlaySelectedFillAlpha = 0.74f;
	FVector2D SurfaceGeometryPadding = FVector2D(10.0f, 10.0f);
	float FlashTickDecay = 1.8f;
	int32 DebugMaxMessages = 200;
	bool bOverlayFilterCurrentAssetOnly = true;
};
