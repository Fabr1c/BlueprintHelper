#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeCommandDescriptor
{
	FString Command;
	TArray<FString> CapabilityDescriptorIds;
	TArray<FString> RuntimeAdapterIds;
	TArray<FString> RoutingHandlerIds;
	EBlueprintHelperBridgeRouteCluster RouteCluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	FString SourceId;
	FString PolicyId;
	bool bAgentVisible = false;
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
