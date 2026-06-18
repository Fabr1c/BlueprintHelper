#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"

static FBlueprintHelperWriteFamilyDescriptor BlueprintHelperWriteFamilyMakeActiveDescriptor(
	const FString& WriteFamily,
	const FString& RuntimeAdapterId,
	const FString& TaskSpecStrategy,
	const FString& ClusterFamily,
	const FString& MetricsIdentity,
	const FString& ReadbackProjectionMode)
{
	FBlueprintHelperWriteFamilyDescriptor Descriptor;
	Descriptor.WriteFamily = WriteFamily;
	Descriptor.RuntimeAdapterId = RuntimeAdapterId;
	Descriptor.TaskSpecStrategy = TaskSpecStrategy;
	Descriptor.BridgeCommand = TEXT("execute_task_plan");
	Descriptor.ClusterFamily = ClusterFamily;
	Descriptor.BodyKind = WriteFamily == TEXT("graphwrite") ? TEXT("graph_body") : TEXT("asset_object");
	Descriptor.OwnershipMode = WriteFamily == TEXT("graphwrite") ? TEXT("graphwrite_generated") : TEXT("write_family_owned");
	Descriptor.ExternalAnchorMode = WriteFamily == TEXT("graphwrite") ? TEXT("graph_body_adapter") : TEXT("none");
	Descriptor.DryRunPolicyId = FString::Printf(TEXT("write_family.%s.full_preview"), *WriteFamily);
	Descriptor.ReadbackProjectionMode = ReadbackProjectionMode;
	Descriptor.ResultProjectionPolicyId = FString::Printf(TEXT("task_runtime.write_family.%s"), *WriteFamily);
	Descriptor.MetricsIdentity = MetricsIdentity;
	Descriptor.Status = EBlueprintHelperWriteFamilyCapabilityStatus::Active;
	Descriptor.bSupportsPreviewUnitOfWork = true;
	Descriptor.bRequiresSandboxGraph = WriteFamily == TEXT("graphwrite");
	Descriptor.bRequiresAdapterPreflight = true;
	Descriptor.bAllowsPreviewMutation = false;
	Descriptor.bRequiresRollbackFinalizer = WriteFamily == TEXT("graphwrite");
	Descriptor.bRequiresReadbackProjection = true;
	return Descriptor;
}

static TArray<FBlueprintHelperWriteFamilyDescriptor> BlueprintHelperWriteFamilyBuildDescriptors()
{
	return {
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("graphwrite"),
				TEXT("graphwrite"),
				TEXT("graphwrite_route_descriptor"),
				TEXT("GraphWrite"),
				TEXT("blueprint.write.graphwrite"),
				TEXT("graph_body_adapter")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("asset_factory"),
				TEXT("asset_factory"),
				TEXT("asset_factory"),
				TEXT("AssetFactory"),
				TEXT("blueprint.write.asset_factory"),
				TEXT("asset_factory")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("blueprint_signature"),
				TEXT("blueprint_signature"),
				TEXT("blueprint_signature"),
				TEXT("Signature"),
				TEXT("blueprint.write.signature"),
				TEXT("blueprint_signature")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("blueprint_variables"),
				TEXT("blueprint_variables"),
				TEXT("blueprint_variables"),
				TEXT("BlueprintVariables"),
				TEXT("blueprint.write.variables"),
				TEXT("blueprint_variables")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("class_settings"),
				TEXT("class_settings"),
				TEXT("class_settings"),
				TEXT("ClassSettings"),
				TEXT("blueprint.write.class_settings"),
				TEXT("class_settings")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("blueprint_component"),
				TEXT("blueprint_component"),
				TEXT("blueprint_component"),
				TEXT("Component"),
				TEXT("blueprint.write.component"),
				TEXT("blueprint_component")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("object_property"),
				TEXT("object_property"),
				TEXT("property_strategy"),
				TEXT("ObjectProperty"),
				TEXT("blueprint.write.object_property"),
				TEXT("object_property")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("data_table"),
				TEXT("data_table"),
				TEXT("row_strategy"),
				TEXT("DataTable"),
				TEXT("blueprint.write.data_table"),
				TEXT("data_table")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("umg_widget"),
				TEXT("umg_widget"),
				TEXT("widget_strategy"),
				TEXT("UMGWidget"),
				TEXT("umg.write.umg_widget"),
				TEXT("widget_tree")),
			BlueprintHelperWriteFamilyMakeActiveDescriptor(
				TEXT("material_instance"),
				TEXT("material_instance"),
				TEXT("material_instance_edit"),
				TEXT("MaterialInstance"),
				TEXT("material.write.material_instance"),
				TEXT("material_instance"))
		};
}

static const TArray<FBlueprintHelperWriteFamilyDescriptor>& BlueprintHelperWriteFamilyGetDescriptors()
{
	static const TArray<FBlueprintHelperWriteFamilyDescriptor> Descriptors =
		BlueprintHelperWriteFamilyBuildDescriptors();
	return Descriptors;
}

const TArray<FBlueprintHelperWriteFamilyDescriptor>&
FBlueprintHelperWriteFamilyDescriptorRegistry::GetKnownDescriptors()
{
	return BlueprintHelperWriteFamilyGetDescriptors();
}

bool FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByWriteFamily(
	const FString& WriteFamily,
	FBlueprintHelperWriteFamilyDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperWriteFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.WriteFamily.Equals(WriteFamily, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByClusterFamily(
	const FString& ClusterFamily,
	FBlueprintHelperWriteFamilyDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperWriteFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.ClusterFamily.Equals(ClusterFamily, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperWriteFamilyDescriptorRegistry::StatusToString(
	EBlueprintHelperWriteFamilyCapabilityStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperWriteFamilyCapabilityStatus::Active:
		return TEXT("active");
	case EBlueprintHelperWriteFamilyCapabilityStatus::Hidden:
		return TEXT("hidden");
	case EBlueprintHelperWriteFamilyCapabilityStatus::Planned:
		return TEXT("planned");
	case EBlueprintHelperWriteFamilyCapabilityStatus::Reserved:
		return TEXT("reserved");
	default:
		return TEXT("unknown");
	}
}
