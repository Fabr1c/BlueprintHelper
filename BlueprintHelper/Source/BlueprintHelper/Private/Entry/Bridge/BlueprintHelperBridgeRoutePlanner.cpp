#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

#include "Entry/Bridge/BlueprintHelperBridgeCommandRegistry.h"
#include "Entry/Bridge/BlueprintHelperBridgeSystemCommandRegistry.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.h"

class FBlueprintHelperBridgeRoutePlannerPrivate
{
public:
	static bool ResolveGeneratedCapabilityRoute(
		const FString& Command,
		FBlueprintHelperBridgeRoutePlan& OutPlan)
	{
		FBlueprintHelperBridgeCommandDescriptor Descriptor;
		if (!FBlueprintHelperBridgeCommandRegistry::TryFindDescriptor(Command, Descriptor))
		{
			OutPlan = FBlueprintHelperBridgeRoutePlan();
			return false;
		}

		OutPlan = FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
			Command,
			Descriptor.RouteCluster,
			Descriptor.SourceId,
			Descriptor.PolicyId,
			Descriptor.bAgentVisible,
			Descriptor.bRequiresGameThread);
		return true;
	}
};

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlanner::BuildPlan(const FString& Command)
{
	FBlueprintHelperBridgeRoutePlan GeneratedCapabilityPlan;
	if (FBlueprintHelperBridgeRoutePlannerPrivate::ResolveGeneratedCapabilityRoute(Command, GeneratedCapabilityPlan))
	{
		return GeneratedCapabilityPlan;
	}

	FBlueprintHelperBridgeRouteMetadata GeneratedRouteMetadata;
	if (FBlueprintHelperBridgeRoutePlannerUtils::FindGeneratedUMGRouteForCommand(
		Command,
		GeneratedRouteMetadata))
	{
		return FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(Command, GeneratedRouteMetadata);
	}

	if (FBlueprintHelperBridgeRoutePlannerUtils::FindGeneratedReadContextRouteForCommand(
		Command,
		GeneratedRouteMetadata))
	{
		return FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(Command, GeneratedRouteMetadata);
	}

	FBlueprintHelperBridgeSystemCommandDescriptor SystemDescriptor;
	if (FBlueprintHelperBridgeSystemCommandRegistry::TryFindDescriptor(Command, SystemDescriptor))
	{
		return FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
			Command,
			SystemDescriptor.Cluster,
			FBlueprintHelperBridgeSystemCommandRegistry::GetSourceName(SystemDescriptor.Source),
			SystemDescriptor.PolicyId,
			SystemDescriptor.bAgentVisible,
			SystemDescriptor.bRequiresGameThread);
	}

	return FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
		Command,
		EBlueprintHelperBridgeRouteCluster::Unknown);
}

const TCHAR* FBlueprintHelperBridgeRoutePlanner::GetClusterName(EBlueprintHelperBridgeRouteCluster Cluster)
{
	return FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterName(Cluster);
}
