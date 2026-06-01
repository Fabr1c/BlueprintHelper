#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperEditorFocusService;
class FBlueprintHelperScreenshotCaptureService;

class BLUEPRINTHELPER_API FBlueprintHelperScreenshotBridgeRoutes
{
public:
	FBlueprintHelperScreenshotBridgeRoutes(
		const FBlueprintHelperEditorFocusService& InFocusService,
		const FBlueprintHelperScreenshotCaptureService& InCaptureService);

	static bool IsScreenshotCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	FBlueprintHelperBridgeResponse HandleFocusBlueprintEditorTarget(
		const FBlueprintHelperBridgeRequest& Request) const;
	FBlueprintHelperBridgeResponse HandleCaptureEditorScreenshot(
		const FBlueprintHelperBridgeRequest& Request) const;
	FBlueprintHelperBridgeResponse HandleCaptureFocusedGraphScreenshot(
		const FBlueprintHelperBridgeRequest& Request) const;

	const FBlueprintHelperEditorFocusService& FocusService;
	const FBlueprintHelperScreenshotCaptureService& CaptureService;
};
