// BlueprintHelper Bridge Layer - BlueprintVariables static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperBlueprintVariableService;

class BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariablesBridgeRoutes
{
public:
	explicit FBlueprintHelperBlueprintVariablesBridgeRoutes(
		const FBlueprintHelperBlueprintVariableService& InVariableService);

	static bool IsBlueprintVariablesCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperBlueprintVariableService& VariableService;
};
