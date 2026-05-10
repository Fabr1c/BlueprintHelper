// BlueprintHelper Bridge Layer - AssetFactory static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperAssetFactoryService;

class BLUEPRINTHELPER_API FBlueprintHelperAssetFactoryBridgeRoutes
{
public:
	explicit FBlueprintHelperAssetFactoryBridgeRoutes(const FBlueprintHelperAssetFactoryService& InAssetFactoryService);

	static bool IsAssetFactoryCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperAssetFactoryService& AssetFactoryService;
};
