// BlueprintHelper Bridge Layer - ClassSettings static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperClassSettingsService;

class BLUEPRINTHELPER_API FBlueprintHelperClassSettingsBridgeRoutes
{
public:
	explicit FBlueprintHelperClassSettingsBridgeRoutes(
		const FBlueprintHelperClassSettingsService& InClassSettingsService);

	static bool IsClassSettingsCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperClassSettingsService& ClassSettingsService;
};
