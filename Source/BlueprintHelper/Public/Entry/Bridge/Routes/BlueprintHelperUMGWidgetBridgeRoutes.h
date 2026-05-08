// BlueprintHelper Bridge Layer - UMGWidget static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperWidgetService;

class BLUEPRINTHELPER_API FBlueprintHelperUMGWidgetBridgeRoutes
{
public:
	explicit FBlueprintHelperUMGWidgetBridgeRoutes(const FBlueprintHelperWidgetService& InWidgetService);

	static bool IsUMGWidgetCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperWidgetService& WidgetService;
};
