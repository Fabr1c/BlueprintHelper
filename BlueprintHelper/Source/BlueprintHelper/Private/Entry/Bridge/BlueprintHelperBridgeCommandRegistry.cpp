#include "Entry/Bridge/BlueprintHelperBridgeCommandRegistry.h"
#include "Runtime/Capabilities/BlueprintHelperRuntimeCapabilityStateBuilder.h"
#include "Runtime/Capabilities/BlueprintHelperGeneratedCapabilityRegistry.h"

static EBlueprintHelperBridgeRouteCluster BlueprintHelperResolveCapabilityHandlerCluster(const FString& HandlerId)
{
	if (HandlerId == TEXT("task_runtime"))
	{
		return EBlueprintHelperBridgeRouteCluster::TaskRuntime;
	}
	if (HandlerId == TEXT("debug_case_export"))
	{
		return EBlueprintHelperBridgeRouteCluster::Debug;
	}
	return EBlueprintHelperBridgeRouteCluster::Unknown;
}

static void BlueprintHelperAddUniqueString(TArray<FString>& Values, const FString& Value)
{
	if (!Value.IsEmpty() && !Values.Contains(Value))
	{
		Values.Add(Value);
	}
}

static TArray<FBlueprintHelperBridgeCommandDescriptor> BlueprintHelperBuildDescriptorBackedBridgeCommands()
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
		if (BridgeCommand.IsEmpty())
		{
			continue;
		}

		const EBlueprintHelperBridgeRouteCluster Cluster = BlueprintHelperResolveCapabilityHandlerCluster(HandlerId);
		if (Cluster == EBlueprintHelperBridgeRouteCluster::Unknown)
		{
			continue;
		}

		FBlueprintHelperBridgeCommandDescriptor& Descriptor = DescriptorsByCommand.FindOrAdd(BridgeCommand);
		Descriptor.Command = BridgeCommand;
		Descriptor.RouteCluster = Cluster;
		Descriptor.bRequiresGameThread = true;
		Descriptor.bAllowsGraphWriteValidationPolicy =
			Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite;
		BlueprintHelperAddUniqueString(Descriptor.CapabilityDescriptorIds, FString(Capability.Id));
		BlueprintHelperAddUniqueString(Descriptor.RuntimeAdapterIds, FString(Capability.RuntimeAdapterId));
		BlueprintHelperAddUniqueString(Descriptor.RoutingHandlerIds, HandlerId);
	}

	TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors;
	DescriptorsByCommand.GenerateValueArray(Descriptors);
	return Descriptors;
}

bool FBlueprintHelperBridgeCommandRegistry::TryFindDescriptor(
	const FString& Command,
	FBlueprintHelperBridgeCommandDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperBridgeCommandDescriptor& Descriptor :
		BlueprintHelperBuildDescriptorBackedBridgeCommands())
	{
		if (Command.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}

	const FBlueprintHelperBridgeRoutePlan RoutePlan = FBlueprintHelperBridgeRoutePlanner::BuildPlan(Command);
	if (!RoutePlan.bKnownCommand)
	{
		OutDescriptor = FBlueprintHelperBridgeCommandDescriptor();
		return false;
	}

	OutDescriptor = FBlueprintHelperBridgeCommandDescriptor();
	OutDescriptor.Command = RoutePlan.Command;
	OutDescriptor.RouteCluster = RoutePlan.Cluster;
	OutDescriptor.bRequiresGameThread = RoutePlan.bRequiresGameThread;
	OutDescriptor.bAllowsGraphWriteValidationPolicy =
		RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite;
	return true;
}

TArray<FBlueprintHelperBridgeCommandDescriptor> FBlueprintHelperBridgeCommandRegistry::GetRepresentativeDescriptors()
{
	TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors =
		BlueprintHelperBuildDescriptorBackedBridgeCommands();
	TSet<FString> SeenCommands;
	for (const FBlueprintHelperBridgeCommandDescriptor& Descriptor : Descriptors)
	{
		SeenCommands.Add(Descriptor.Command);
	}

	const FString Commands[] = {
		TEXT("preview_task_plan"),
		TEXT("execute_task_plan"),
		TEXT("append_blueprint_graph"),
		TEXT("merge_external_flow"),
		TEXT("patch_external_graph"),
		TEXT("patch_external_links"),
		TEXT("replace_external_body"),
		TEXT("read_blueprint_logic_json"),
		TEXT("apply_review_action"),
	};

	for (const FString& Command : Commands)
	{
		if (SeenCommands.Contains(Command))
		{
			continue;
		}
		FBlueprintHelperBridgeCommandDescriptor Descriptor;
		if (TryFindDescriptor(Command, Descriptor))
		{
			Descriptors.Add(MoveTemp(Descriptor));
			SeenCommands.Add(Command);
		}
	}
	return Descriptors;
}
