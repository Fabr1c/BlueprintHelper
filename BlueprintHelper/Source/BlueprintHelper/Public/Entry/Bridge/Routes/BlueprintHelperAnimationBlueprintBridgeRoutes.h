// BlueprintHelper Bridge Layer - AnimationBlueprint static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperAnimationBlueprintBridgeRoutes
{
public:
	static bool IsAnimationBlueprintCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;
};
