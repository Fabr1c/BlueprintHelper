#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"

static FBlueprintHelperGraphBodyAdapterDescriptor BlueprintHelperMakeGraphBodyDescriptor(
	const TCHAR* RuntimeAdapterId,
	const TCHAR* TaskSpecStrategy,
	EBlueprintHelperGraphBodyKind BodyKind,
	const TCHAR* BoundarySource,
	bool bSupportsDryRunUnitOfWork,
	bool bSupportsExternalAnchors,
	bool bReservedOnly = false)
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	Descriptor.RuntimeAdapterId = RuntimeAdapterId;
	Descriptor.TaskSpecStrategy = TaskSpecStrategy;
	Descriptor.BodyKind = BodyKind;
	Descriptor.BoundarySource = BoundarySource;
	Descriptor.bSupportsDryRunUnitOfWork = bSupportsDryRunUnitOfWork;
	Descriptor.bSupportsExternalAnchors = bSupportsExternalAnchors;
	Descriptor.bReservedOnly = bReservedOnly;
	return Descriptor;
}

TArray<FBlueprintHelperGraphBodyAdapterDescriptor> FBlueprintHelperGraphBodyAdapterRegistry::GetKnownDescriptors()
{
	TArray<FBlueprintHelperGraphBodyAdapterDescriptor> Descriptors;
	Descriptors.Reserve(8);
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.custom_event_body"),
		TEXT("append_new_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2CustomEventBody,
		TEXT("custom_event_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.event_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2EventBody,
		TEXT("event_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.function_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2FunctionBody,
		TEXT("function_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.macro_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2MacroBody,
		TEXT("macro_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.block_implementation"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2BlockImplementation,
		TEXT("block_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_body"),
		TEXT("merge_external_flow"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_body_adapter"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("material.function_body"),
		TEXT(""),
		EBlueprintHelperGraphBodyKind::ReservedMaterialFunctionBody,
		TEXT("reserved"),
		false,
		false,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("animation.graph_body"),
		TEXT(""),
		EBlueprintHelperGraphBodyKind::ReservedAnimationGraphBody,
		TEXT("reserved"),
		false,
		false,
		true));
	return Descriptors;
}

bool FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(
	const FString& RuntimeAdapterId,
	FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.RuntimeAdapterId.Equals(RuntimeAdapterId, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperGraphBodyAdapterRegistry::TryFindByTaskSpecStrategy(
	const FString& TaskSpecStrategy,
	FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor)
{
	if (TaskSpecStrategy.IsEmpty())
	{
		return false;
	}
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (!Descriptor.TaskSpecStrategy.IsEmpty() &&
			Descriptor.TaskSpecStrategy.Equals(TaskSpecStrategy, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}
