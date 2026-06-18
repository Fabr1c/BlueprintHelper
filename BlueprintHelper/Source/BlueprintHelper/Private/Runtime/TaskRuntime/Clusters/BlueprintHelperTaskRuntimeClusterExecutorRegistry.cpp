#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterExecutorRegistry.h"

#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterFamilyRegistry.h"

TUniquePtr<FBlueprintHelperTaskRuntimeClusterExecutorRegistry>
FBlueprintHelperTaskRuntimeClusterExecutorRegistry::CreateDefault(
	const FBlueprintHelperGraphWriteTaskRuntimeCluster& GraphWriteCluster,
	const FBlueprintHelperBlueprintVariablesTaskRuntimeCluster& BlueprintVariablesCluster,
	const FBlueprintHelperAssetFactoryTaskRuntimeCluster& AssetFactoryCluster,
	const FBlueprintHelperComponentTaskRuntimeCluster& ComponentCluster,
	const FBlueprintHelperClassSettingsTaskRuntimeCluster& ClassSettingsCluster,
	const FBlueprintHelperSignatureTaskRuntimeCluster& SignatureCluster,
	const FBlueprintHelperUMGWidgetTaskRuntimeCluster& UMGWidgetCluster,
	const FBlueprintHelperDataTableTaskRuntimeCluster& DataTableCluster,
	const FBlueprintHelperObjectPropertyTaskRuntimeCluster& ObjectPropertyCluster,
	const FBlueprintHelperMaterialInstanceTaskRuntimeCluster& MaterialInstanceCluster)
{
	TUniquePtr<FBlueprintHelperTaskRuntimeClusterExecutorRegistry> Registry =
		MakeUnique<FBlueprintHelperTaskRuntimeClusterExecutorRegistry>();
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::GraphWrite,
		[&GraphWriteCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return GraphWriteCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
		[&BlueprintVariablesCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return BlueprintVariablesCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::AssetFactory,
		[&AssetFactoryCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return AssetFactoryCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::Component,
		[&ComponentCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return ComponentCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::ClassSettings,
		[&ClassSettingsCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return ClassSettingsCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::Signature,
		[&SignatureCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return SignatureCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::UMGWidget,
		[&UMGWidgetCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return UMGWidgetCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::DataTable,
		[&DataTableCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return DataTableCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
		[&ObjectPropertyCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return ObjectPropertyCluster.ExecuteStep(LoweredStep);
		});
	Registry->Register(
		EBlueprintHelperTaskRuntimeCluster::MaterialInstance,
		[&MaterialInstanceCluster](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
		{
			return MaterialInstanceCluster.ExecuteStep(LoweredStep);
		});
	return Registry;
}

const FBlueprintHelperTaskRuntimeClusterExecutor*
FBlueprintHelperTaskRuntimeClusterExecutorRegistry::FindByCluster(
	EBlueprintHelperTaskRuntimeCluster Cluster) const
{
	for (const FBlueprintHelperTaskRuntimeClusterExecutor& Executor : Executors)
	{
		if (Executor.Cluster == Cluster && Executor.ExecuteStep)
		{
			return &Executor;
		}
	}
	return nullptr;
}

bool FBlueprintHelperTaskRuntimeClusterExecutorRegistry::CanExecuteCluster(
	EBlueprintHelperTaskRuntimeCluster Cluster) const
{
	return FindByCluster(Cluster) != nullptr;
}

void FBlueprintHelperTaskRuntimeClusterExecutorRegistry::Register(
	EBlueprintHelperTaskRuntimeCluster Cluster,
	TFunction<FBlueprintHelperToolResultBase(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)> ExecuteStep)
{
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor Descriptor;
	if (!FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByCluster(Cluster, Descriptor) || !ExecuteStep)
	{
		return;
	}

	FBlueprintHelperTaskRuntimeClusterExecutor Executor;
	Executor.Cluster = Cluster;
	Executor.ExecuteStep = MoveTemp(ExecuteStep);
	Executors.Add(MoveTemp(Executor));
}
