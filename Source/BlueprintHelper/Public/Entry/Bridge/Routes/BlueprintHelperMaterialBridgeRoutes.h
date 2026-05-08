// BlueprintHelper Bridge Layer - Material static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialBridgeRoutes
{
public:
	static bool IsMaterialCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;
};
