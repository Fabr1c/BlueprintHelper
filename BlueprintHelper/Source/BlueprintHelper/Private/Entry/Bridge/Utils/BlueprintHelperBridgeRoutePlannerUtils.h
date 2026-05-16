#pragma once

#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

class FBlueprintHelperBridgeRoutePlannerUtils
{
public:
	static FBlueprintHelperBridgeRoutePlan MakePlan(
		const FString& Command,
		EBlueprintHelperBridgeRouteCluster Cluster);

	static EBlueprintHelperBridgeRouteCluster FindClusterForCommand(const FString& Command);

	static const TCHAR* ResolveClusterName(EBlueprintHelperBridgeRouteCluster Cluster);
};
