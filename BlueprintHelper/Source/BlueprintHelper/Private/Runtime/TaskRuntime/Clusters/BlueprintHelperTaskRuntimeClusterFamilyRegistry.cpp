#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterFamilyRegistry.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/AssetFactory/BlueprintHelperAssetFactoryTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/DataTable/BlueprintHelperDataTableTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"

class FBlueprintHelperTaskRuntimeClusterFamilyCatalog
{
public:
	static const TArray<FBlueprintHelperTaskRuntimeClusterFamilyDescriptor>& Get()
	{
		static const TArray<FBlueprintHelperTaskRuntimeClusterFamilyDescriptor> Descriptors = {
			{EBlueprintHelperTaskRuntimeCluster::GraphWrite, TEXT("GraphWrite"), TEXT("graphwrite"), &FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::BlueprintVariables, TEXT("BlueprintVariables"), TEXT("blueprint_variables"), &FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::AssetFactory, TEXT("AssetFactory"), TEXT("asset_factory"), &FBlueprintHelperAssetFactoryTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::Component, TEXT("Component"), TEXT("blueprint_component"), &FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::ClassSettings, TEXT("ClassSettings"), TEXT("class_settings"), &FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::Signature, TEXT("Signature"), TEXT("blueprint_signature"), &FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::UMGWidget, TEXT("UMGWidget"), TEXT("umg_widget"), &FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::DataTable, TEXT("DataTable"), TEXT("data_table"), &FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep},
			{EBlueprintHelperTaskRuntimeCluster::ObjectProperty, TEXT("ObjectProperty"), TEXT("object_property"), &FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep}
		};
		return Descriptors;
	}
};

const TArray<FBlueprintHelperTaskRuntimeClusterFamilyDescriptor>&
FBlueprintHelperTaskRuntimeClusterFamilyRegistry::GetKnownDescriptors()
{
	return FBlueprintHelperTaskRuntimeClusterFamilyCatalog::Get();
}

TArray<EBlueprintHelperTaskRuntimeCluster>
FBlueprintHelperTaskRuntimeClusterFamilyRegistry::GetRegisteredClusters()
{
	TArray<EBlueprintHelperTaskRuntimeCluster> Clusters;
	for (const FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		Clusters.Add(Descriptor.Cluster);
	}
	return Clusters;
}

bool FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByCluster(
	EBlueprintHelperTaskRuntimeCluster Cluster,
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.Cluster == Cluster)
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByWriteFamily(
	const FString& WriteFamily,
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.WriteFamily.Equals(WriteFamily, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

EBlueprintHelperTaskRuntimeCluster
FBlueprintHelperTaskRuntimeClusterFamilyRegistry::ResolveClusterForLoweredStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	for (const FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.CanExecuteStep && Descriptor.CanExecuteStep(LoweredStep))
		{
			return Descriptor.Cluster;
		}
	}
	return EBlueprintHelperTaskRuntimeCluster::Unknown;
}
