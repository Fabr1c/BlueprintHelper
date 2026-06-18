#include "Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.h"

#include "Generated/BlueprintHelperReadContextRouteManifest.generated.h"
#include "Generated/BlueprintHelperUMGWidgetOperationManifest.generated.h"

static const TPair<EBlueprintHelperBridgeRouteCluster, const TCHAR*> GBlueprintHelperBridgeRouteClusterNames[] = {
	{EBlueprintHelperBridgeRouteCluster::Core, TEXT("Core")},
	{EBlueprintHelperBridgeRouteCluster::Debug, TEXT("Debug")},
	{EBlueprintHelperBridgeRouteCluster::SharedServices, TEXT("SharedServices")},
	{EBlueprintHelperBridgeRouteCluster::AssetBrowser, TEXT("AssetBrowser")},
	{EBlueprintHelperBridgeRouteCluster::AssetDiscovery, TEXT("AssetDiscovery")},
	{EBlueprintHelperBridgeRouteCluster::TaskRuntime, TEXT("TaskRuntime")},
	{EBlueprintHelperBridgeRouteCluster::GraphWrite, TEXT("GraphWrite")},
	{EBlueprintHelperBridgeRouteCluster::BlueprintVariables, TEXT("BlueprintVariables")},
	{EBlueprintHelperBridgeRouteCluster::BlueprintStructure, TEXT("BlueprintStructure")},
	{EBlueprintHelperBridgeRouteCluster::AssetFactory, TEXT("AssetFactory")},
	{EBlueprintHelperBridgeRouteCluster::Component, TEXT("Component")},
	{EBlueprintHelperBridgeRouteCluster::ClassSettings, TEXT("ClassSettings")},
	{EBlueprintHelperBridgeRouteCluster::Signature, TEXT("Signature")},
	{EBlueprintHelperBridgeRouteCluster::UMGWidget, TEXT("UMGWidget")},
	{EBlueprintHelperBridgeRouteCluster::DataTable, TEXT("DataTable")},
	{EBlueprintHelperBridgeRouteCluster::ObjectProperty, TEXT("ObjectProperty")},
	{EBlueprintHelperBridgeRouteCluster::EditorCommand, TEXT("EditorCommand")},
	{EBlueprintHelperBridgeRouteCluster::Review, TEXT("Review")},
};

class FBlueprintHelperGeneratedRouteClusterUtils
{
public:
	static EBlueprintHelperBridgeRouteCluster ResolveClusterFromGeneratedName(const TCHAR* ClusterName)
	{
		return FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterFromName(
			ClusterName ? FString(ClusterName) : FString());
	}

	static bool FindGeneratedUmgRoute(
		const FString& Command,
		FBlueprintHelperBridgeRouteMetadata& OutMetadata)
	{
		for (const FBlueprintHelperGeneratedCommandDescriptor& Descriptor : GBlueprintHelperUMGWidgetOperationCommands)
		{
			if (Command.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
			{
				OutMetadata.Cluster = ResolveClusterFromGeneratedName(Descriptor.Cluster);
				OutMetadata.SourceId = FString(Descriptor.RouteSourceId);
				OutMetadata.PolicyId = FString(Descriptor.RoutePolicyId);
				OutMetadata.bAgentVisible = Descriptor.bRouteAgentVisible;
				OutMetadata.bRequiresGameThread = Descriptor.bRouteRequiresGameThread;
				return OutMetadata.Cluster != EBlueprintHelperBridgeRouteCluster::Unknown;
			}
		}
		OutMetadata = FBlueprintHelperBridgeRouteMetadata();
		return false;
	}

	static bool FindGeneratedReadContextRoute(
		const FString& Command,
		FBlueprintHelperBridgeRouteMetadata& OutMetadata)
	{
		for (const FBlueprintHelperGeneratedReadContextRouteDescriptor& Descriptor : GBlueprintHelperReadContextRoutes)
		{
			if (FString(Descriptor.Status).Equals(TEXT("active"), ESearchCase::IgnoreCase) &&
				!FString(Descriptor.Command).IsEmpty() &&
				Command.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
			{
				OutMetadata.Cluster = ResolveClusterFromGeneratedName(Descriptor.Cluster);
				OutMetadata.SourceId = FString(Descriptor.RouteSourceId);
				OutMetadata.PolicyId = FString(Descriptor.RoutePolicyId);
				OutMetadata.bAgentVisible = Descriptor.bRouteAgentVisible;
				OutMetadata.bRequiresGameThread = Descriptor.bRouteRequiresGameThread;
				return OutMetadata.Cluster != EBlueprintHelperBridgeRouteCluster::Unknown;
			}
		}
		OutMetadata = FBlueprintHelperBridgeRouteMetadata();
		return false;
	}
};

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
	const FString& Command,
	EBlueprintHelperBridgeRouteCluster Cluster,
	const FString& SourceId,
	const FString& PolicyId,
	bool bAgentVisible,
	bool bRequiresGameThread)
{
	FBlueprintHelperBridgeRoutePlan Plan;
	Plan.Command = Command;
	Plan.SourceId = SourceId;
	Plan.PolicyId = PolicyId;
	Plan.Cluster = Cluster;
	Plan.bKnownCommand = Cluster != EBlueprintHelperBridgeRouteCluster::Unknown;
	Plan.bRequiresGameThread = Plan.bKnownCommand && bRequiresGameThread;
	Plan.bAgentVisible = bAgentVisible;
	return Plan;
}

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
	const FString& Command,
	const FBlueprintHelperBridgeRouteMetadata& Metadata)
{
	return MakePlan(
		Command,
		Metadata.Cluster,
		Metadata.SourceId,
		Metadata.PolicyId,
		Metadata.bAgentVisible,
		Metadata.bRequiresGameThread);
}

bool FBlueprintHelperBridgeRoutePlannerUtils::FindGeneratedUMGRouteForCommand(
	const FString& Command,
	FBlueprintHelperBridgeRouteMetadata& OutMetadata)
{
	return FBlueprintHelperGeneratedRouteClusterUtils::FindGeneratedUmgRoute(Command, OutMetadata);
}

bool FBlueprintHelperBridgeRoutePlannerUtils::FindGeneratedReadContextRouteForCommand(
	const FString& Command,
	FBlueprintHelperBridgeRouteMetadata& OutMetadata)
{
	return FBlueprintHelperGeneratedRouteClusterUtils::FindGeneratedReadContextRoute(Command, OutMetadata);
}

EBlueprintHelperBridgeRouteCluster FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterFromName(
	const FString& ClusterName)
{
	for (const TPair<EBlueprintHelperBridgeRouteCluster, const TCHAR*>& Entry : GBlueprintHelperBridgeRouteClusterNames)
	{
		if (ClusterName.Equals(Entry.Value, ESearchCase::IgnoreCase))
		{
			return Entry.Key;
		}
	}
	return EBlueprintHelperBridgeRouteCluster::Unknown;
}

const TCHAR* FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterName(EBlueprintHelperBridgeRouteCluster Cluster)
{
	for (const TPair<EBlueprintHelperBridgeRouteCluster, const TCHAR*>& Entry : GBlueprintHelperBridgeRouteClusterNames)
	{
		if (Cluster == Entry.Key)
		{
			return Entry.Value;
		}
	}
	return TEXT("Unknown");
}
