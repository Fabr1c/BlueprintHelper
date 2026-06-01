#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperScreenshotSettings
{
	FString OutputDir = TEXT("Screenshots");
	EBlueprintHelperScreenshotTarget DefaultCaptureTarget = EBlueprintHelperScreenshotTarget::ActiveWindow;
	FString FilenamePrefix = TEXT("editor");
	int32 GraphMaxNodesPerImage = 8;

	static FBlueprintHelperScreenshotSettings Load();
};
