// BlueprintHelper Bridge Layer - AssetDiscovery static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperAssetDiscoveryService;

class BLUEPRINTHELPER_API FBlueprintHelperAssetDiscoveryBridgeRoutes
{
public:
	explicit FBlueprintHelperAssetDiscoveryBridgeRoutes(const FBlueprintHelperAssetDiscoveryService& InAssetDiscoveryService);

	static bool IsAssetDiscoveryCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperAssetDiscoveryService& AssetDiscoveryService;
};
