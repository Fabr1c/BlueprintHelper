// BlueprintHelper Bridge Layer - GraphWrite static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperGraphWriteServiceRegistry;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteBridgeRoutes
{
public:
	explicit FBlueprintHelperGraphWriteBridgeRoutes(
		const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry);

	static bool IsGraphWriteCommand(const FString& Command);
	static bool IsGraphWriteReadCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperGraphWriteServiceRegistry& GraphWriteRegistry;
};
