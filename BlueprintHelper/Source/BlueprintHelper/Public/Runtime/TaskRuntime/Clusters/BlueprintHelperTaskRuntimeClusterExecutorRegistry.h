#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeClusterExecutorRegistry
{
public:
	static TUniquePtr<FBlueprintHelperTaskRuntimeClusterExecutorRegistry> CreateDefault(
		const FBlueprintHelperGraphWriteTaskRuntimeCluster& GraphWriteCluster,
		const FBlueprintHelperBlueprintVariablesTaskRuntimeCluster& BlueprintVariablesCluster,
		const FBlueprintHelperAssetFactoryTaskRuntimeCluster& AssetFactoryCluster,
		const FBlueprintHelperComponentTaskRuntimeCluster& ComponentCluster,
		const FBlueprintHelperClassSettingsTaskRuntimeCluster& ClassSettingsCluster,
		const FBlueprintHelperSignatureTaskRuntimeCluster& SignatureCluster,
		const FBlueprintHelperUMGWidgetTaskRuntimeCluster& UMGWidgetCluster,
		const FBlueprintHelperDataTableTaskRuntimeCluster& DataTableCluster,
		const FBlueprintHelperObjectPropertyTaskRuntimeCluster& ObjectPropertyCluster,
		const FBlueprintHelperMaterialInstanceTaskRuntimeCluster& MaterialInstanceCluster);

	const FBlueprintHelperTaskRuntimeClusterExecutor* FindByCluster(
		EBlueprintHelperTaskRuntimeCluster Cluster) const;
	bool CanExecuteCluster(EBlueprintHelperTaskRuntimeCluster Cluster) const;

private:
	void Register(
		EBlueprintHelperTaskRuntimeCluster Cluster,
		TFunction<FBlueprintHelperToolResultBase(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)> ExecuteStep);

	TArray<FBlueprintHelperTaskRuntimeClusterExecutor> Executors;
};
