#include "Entry/Bridge/BlueprintHelperBridgeCommandRegistry.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.h"
#include "Runtime/Capabilities/BlueprintHelperRuntimeCapabilityStateBuilder.h"
#include "Runtime/Capabilities/BlueprintHelperGeneratedCapabilityRegistry.h"

class FBlueprintHelperBridgeCommandRegistryPrivate
{
public:
	static void AddUniqueString(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty() && !Values.Contains(Value))
		{
			Values.Add(Value);
		}
	}

	static TArray<FBlueprintHelperBridgeCommandDescriptor> BuildDescriptorBackedBridgeCommands()
	{
		const FBlueprintHelperCapabilityDescriptorRuntimeState RuntimeState =
			FBlueprintHelperRuntimeCapabilityStateBuilder::BuildRegisteredRuntimeState();
		TMap<FString, FBlueprintHelperBridgeCommandDescriptor> DescriptorsByCommand;
		for (const FBlueprintHelperGeneratedCapabilityDescriptor& Capability :
			FBlueprintHelperGeneratedCapabilityRegistry::ListDescriptors())
		{
			if (!FBlueprintHelperGeneratedCapabilityRegistry::IsDescriptorAgentVisible(Capability, RuntimeState))
			{
				continue;
			}

			const FString BridgeCommand(Capability.RoutingBridgeCommand);
			const FString HandlerId(Capability.RoutingHandlerId);
			const FString RouteClusterName(Capability.RoutingRouteCluster);
			if (BridgeCommand.IsEmpty())
			{
				continue;
			}

			const EBlueprintHelperBridgeRouteCluster Cluster =
				FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterFromName(RouteClusterName);
			if (Cluster == EBlueprintHelperBridgeRouteCluster::Unknown)
			{
				continue;
			}

			FBlueprintHelperBridgeCommandDescriptor& Descriptor = DescriptorsByCommand.FindOrAdd(BridgeCommand);
			Descriptor.Command = BridgeCommand;
			Descriptor.RouteCluster = Cluster;
			Descriptor.SourceId = FString(Capability.RoutingSourceId);
			Descriptor.PolicyId = FString(Capability.RoutingPolicyId);
			Descriptor.bAgentVisible = Capability.bRoutingAgentVisible;
			Descriptor.bRequiresGameThread = Capability.bRoutingRequiresGameThread;
			Descriptor.bAllowsGraphWriteValidationPolicy =
				Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite;
			AddUniqueString(Descriptor.CapabilityDescriptorIds, FString(Capability.Id));
			AddUniqueString(Descriptor.RuntimeAdapterIds, FString(Capability.RuntimeAdapterId));
			AddUniqueString(Descriptor.RoutingHandlerIds, HandlerId);
		}

		TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors;
		DescriptorsByCommand.GenerateValueArray(Descriptors);
		return Descriptors;
	}

	static const TArray<FBlueprintHelperBridgeCommandDescriptor>& GetDescriptorBackedBridgeCommands()
	{
		static const TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors =
			BuildDescriptorBackedBridgeCommands();
		return Descriptors;
	}
};

bool FBlueprintHelperBridgeCommandRegistry::TryFindDescriptor(
	const FString& Command,
	FBlueprintHelperBridgeCommandDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperBridgeCommandDescriptor& Descriptor :
		FBlueprintHelperBridgeCommandRegistryPrivate::GetDescriptorBackedBridgeCommands())
	{
		if (Command.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}

	OutDescriptor = FBlueprintHelperBridgeCommandDescriptor();
	return false;
}

TArray<FBlueprintHelperBridgeCommandDescriptor> FBlueprintHelperBridgeCommandRegistry::GetRepresentativeDescriptors()
{
	return FBlueprintHelperBridgeCommandRegistryPrivate::GetDescriptorBackedBridgeCommands();
}
