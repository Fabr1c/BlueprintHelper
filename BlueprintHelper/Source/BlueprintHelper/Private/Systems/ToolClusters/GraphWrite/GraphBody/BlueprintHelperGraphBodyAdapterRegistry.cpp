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
		TEXT("append_blueprint_graph"),
		TEXT("append_new_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2CustomEventBody,
		TEXT("append_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("replace_blueprint_graph"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2FunctionBody,
		TEXT("replace_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("patch_blueprint_graph"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2BlockImplementation,
		TEXT("owned_patch_policy"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("merge_blueprint_graph"),
		TEXT("merge_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2BlockImplementation,
		TEXT("merge_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("merge_external_flow"),
		TEXT("merge_external_flow"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_anchor_resolver"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("patch_external_graph"),
		TEXT("patch_external_graph"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_anchor_resolver"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("replace_external_body"),
		TEXT("replace_external_body"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_body_snapshot"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.macro_body"),
		TEXT(""),
		EBlueprintHelperGraphBodyKind::ReservedMacroBody,
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
