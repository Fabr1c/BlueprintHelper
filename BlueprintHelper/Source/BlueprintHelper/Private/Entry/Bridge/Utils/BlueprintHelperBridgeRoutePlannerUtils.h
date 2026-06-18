#pragma once

#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

struct FBlueprintHelperBridgeRouteMetadata
{
	EBlueprintHelperBridgeRouteCluster Cluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	FString SourceId;
	FString PolicyId;
	bool bAgentVisible = false;
	bool bRequiresGameThread = true;
};

class FBlueprintHelperBridgeRoutePlannerUtils
{
public:
	static FBlueprintHelperBridgeRoutePlan MakePlan(
		const FString& Command,
		EBlueprintHelperBridgeRouteCluster Cluster,
		const FString& SourceId = FString(),
		const FString& PolicyId = FString(),
		bool bAgentVisible = false,
		bool bRequiresGameThread = true);

	static FBlueprintHelperBridgeRoutePlan MakePlan(
		const FString& Command,
		const FBlueprintHelperBridgeRouteMetadata& Metadata);

	static bool FindGeneratedUMGRouteForCommand(
		const FString& Command,
		FBlueprintHelperBridgeRouteMetadata& OutMetadata);

	static bool FindGeneratedReadContextRouteForCommand(
		const FString& Command,
		FBlueprintHelperBridgeRouteMetadata& OutMetadata);

	static EBlueprintHelperBridgeRouteCluster ResolveClusterFromName(const FString& ClusterName);

	static const TCHAR* ResolveClusterName(EBlueprintHelperBridgeRouteCluster Cluster);
};
