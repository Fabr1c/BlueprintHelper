#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeCommandDescriptor
{
	FString Command;
	EBlueprintHelperBridgeRouteCluster RouteCluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	bool bRequiresGameThread = true;
	bool bAllowsGraphWriteValidationPolicy = false;
	TFunction<FBlueprintHelperBridgeResponse(const FBlueprintHelperBridgeRequest&)> Handler;
};

class BLUEPRINTHELPER_API FBlueprintHelperBridgeCommandRegistry
{
public:
	static bool TryFindDescriptor(
		const FString& Command,
		FBlueprintHelperBridgeCommandDescriptor& OutDescriptor);
	static TArray<FBlueprintHelperBridgeCommandDescriptor> GetRepresentativeDescriptors();
};
