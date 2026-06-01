#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperScreenshotCaptureService
{
public:
	FBlueprintHelperScreenshotCaptureResult Capture(
		const FBlueprintHelperScreenshotCaptureRequest& Request) const;
	FBlueprintHelperGraphScreenshotCaptureResult CaptureFocusedGraph(
		const FBlueprintHelperGraphScreenshotCaptureRequest& Request) const;

	static FString BuildOutputDirectory();
	static FString SanitizeFileLabel(const FString& Label, const FString& Fallback);

private:
	FBlueprintHelperScreenshotCaptureResult CaptureActiveWindow(
		const FBlueprintHelperScreenshotCaptureRequest& Request) const;
	FBlueprintHelperScreenshotCaptureResult CaptureActiveViewport(
		const FBlueprintHelperScreenshotCaptureRequest& Request) const;
	FBlueprintHelperScreenshotCaptureResult SavePixels(
		const FBlueprintHelperScreenshotCaptureRequest& Request,
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height) const;
};
