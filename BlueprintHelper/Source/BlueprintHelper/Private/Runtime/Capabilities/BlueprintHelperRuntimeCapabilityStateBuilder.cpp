#include "Runtime/Capabilities/BlueprintHelperRuntimeCapabilityStateBuilder.h"

#include "Runtime/Capabilities/BlueprintHelperGeneratedCapabilityRegistry.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterFamilyRegistry.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"

static void BlueprintHelperRuntimeStateAddRegisteredAdapter(
	FBlueprintHelperCapabilityDescriptorRuntimeState& RuntimeState,
	const FString& RuntimeAdapterId)
{
	if (!RuntimeAdapterId.IsEmpty())
	{
		RuntimeState.RegisteredRuntimeAdapterIds.Add(RuntimeAdapterId);
	}
}

static FString BlueprintHelperRuntimeStateResolveTaskRuntimeWriteFamily(
	const FString& CapabilityFamily)
{
	if (CapabilityFamily == TEXT("umg_widget_tree"))
	{
		return TEXT("umg_widget");
	}
	return CapabilityFamily;
}

static bool BlueprintHelperRuntimeStateHasRegisteredTaskRuntimeWriteFamily(
	const FString& WriteFamily)
{
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor ClusterDescriptor;
	TSharedPtr<IBlueprintHelperWriteFamilyAdapter> WriteFamilyAdapter;
	return FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByWriteFamily(
			WriteFamily,
			ClusterDescriptor) &&
		FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByWriteFamily(
			WriteFamily,
			WriteFamilyAdapter) &&
		WriteFamilyAdapter.IsValid();
}

static bool BlueprintHelperRuntimeStateHasRegisteredMaterialGraphRuntime()
{
	return BlueprintHelperRuntimeStateHasRegisteredTaskRuntimeWriteFamily(TEXT("graphwrite")) &&
		FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(TEXT("material_graph_edit"));
}

static bool BlueprintHelperRuntimeStateHasRegisteredTaskRuntimeAdapter(
	const FBlueprintHelperGeneratedCapabilityDescriptor& Descriptor)
{
	const FString CapabilityFamily(Descriptor.Family);
	if (CapabilityFamily == TEXT("material_graph"))
	{
		return BlueprintHelperRuntimeStateHasRegisteredMaterialGraphRuntime();
	}

	const FString WriteFamily =
		BlueprintHelperRuntimeStateResolveTaskRuntimeWriteFamily(CapabilityFamily);
	return BlueprintHelperRuntimeStateHasRegisteredTaskRuntimeWriteFamily(WriteFamily);
}

static bool BlueprintHelperRuntimeStateHasRegisteredDebugCaseExportAdapter(
	const FBlueprintHelperGeneratedCapabilityDescriptor& Descriptor)
{
	return FString(Descriptor.RoutingHandlerId) == TEXT("debug_case_export") &&
		FString(Descriptor.RoutingBridgeCommand) == TEXT("export_debug_bundle");
}

static bool BlueprintHelperRuntimeStateHasRegisteredRuntimeAdapter(
	const FBlueprintHelperGeneratedCapabilityDescriptor& Descriptor)
{
	const FString HandlerId(Descriptor.RoutingHandlerId);
	if (HandlerId == TEXT("task_runtime"))
	{
		return BlueprintHelperRuntimeStateHasRegisteredTaskRuntimeAdapter(Descriptor);
	}
	if (HandlerId == TEXT("debug_case_export"))
	{
		return BlueprintHelperRuntimeStateHasRegisteredDebugCaseExportAdapter(Descriptor);
	}
	return false;
}

FBlueprintHelperCapabilityDescriptorRuntimeState
FBlueprintHelperRuntimeCapabilityStateBuilder::BuildRegisteredRuntimeState()
{
	FBlueprintHelperCapabilityDescriptorRuntimeState RuntimeState;
	RuntimeState.bAllowWriteCapabilities = true;
	RuntimeState.bAllowHighRiskCapabilities = true;

	for (const FBlueprintHelperGeneratedCapabilityDescriptor& Descriptor :
		FBlueprintHelperGeneratedCapabilityRegistry::ListDescriptors())
	{
		if (BlueprintHelperRuntimeStateHasRegisteredRuntimeAdapter(Descriptor))
		{
			BlueprintHelperRuntimeStateAddRegisteredAdapter(
				RuntimeState,
				FString(Descriptor.RuntimeAdapterId));
		}
	}

	return RuntimeState;
}
