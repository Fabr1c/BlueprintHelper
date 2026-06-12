#include "Entry/Bridge/BlueprintHelperBridgeCommandRegistry.h"

bool FBlueprintHelperBridgeCommandRegistry::TryFindDescriptor(
	const FString& Command,
	FBlueprintHelperBridgeCommandDescriptor& OutDescriptor)
{
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

	TArray<FBlueprintHelperBridgeCommandDescriptor> Descriptors;
	for (const FString& Command : Commands)
	{
		FBlueprintHelperBridgeCommandDescriptor Descriptor;
		if (TryFindDescriptor(Command, Descriptor))
		{
			Descriptors.Add(MoveTemp(Descriptor));
		}
	}
	return Descriptors;
}
