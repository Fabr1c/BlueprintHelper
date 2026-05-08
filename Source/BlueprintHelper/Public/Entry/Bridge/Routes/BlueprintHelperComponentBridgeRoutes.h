// BlueprintHelper Bridge Layer - Component static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperComponentService;

class BLUEPRINTHELPER_API FBlueprintHelperComponentBridgeRoutes
{
public:
	explicit FBlueprintHelperComponentBridgeRoutes(const FBlueprintHelperComponentService& InComponentService);

	static bool IsComponentCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperComponentService& ComponentService;
};
