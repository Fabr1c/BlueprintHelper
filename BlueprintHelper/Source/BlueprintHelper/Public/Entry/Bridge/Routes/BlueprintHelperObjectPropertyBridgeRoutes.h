// BlueprintHelper Bridge Layer - ObjectProperty static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperPropertyReflectionService;

class BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyBridgeRoutes
{
public:
	explicit FBlueprintHelperObjectPropertyBridgeRoutes(
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService);

	static bool IsObjectPropertyCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperPropertyReflectionService& PropertyReflectionService;
};
