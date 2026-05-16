#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

#include "Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.h"

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlanner::BuildPlan(const FString& Command)
{
	const EBlueprintHelperBridgeRouteCluster Cluster =
		FBlueprintHelperBridgeRoutePlannerUtils::FindClusterForCommand(Command);
	return FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(Command, Cluster);
}

const TCHAR* FBlueprintHelperBridgeRoutePlanner::GetClusterName(EBlueprintHelperBridgeRouteCluster Cluster)
{
	return FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterName(Cluster);
}
