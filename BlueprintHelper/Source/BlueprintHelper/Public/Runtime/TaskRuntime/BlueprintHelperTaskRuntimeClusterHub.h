// BlueprintHelper TaskRuntime - static tool cluster hub

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Clusters/AssetFactory/BlueprintHelperAssetFactoryTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/DataTable/BlueprintHelperDataTableTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperGraphWriteServiceRegistry;
class FBlueprintHelperBlueprintVariableService;
class FBlueprintHelperBlueprintStructureService;
class FBlueprintHelperAssetFactoryService;
class FBlueprintHelperComponentService;
class FBlueprintHelperClassSettingsService;
class FBlueprintHelperWidgetService;
class FBlueprintHelperDataTableService;
class FBlueprintHelperPropertyReflectionService;
struct FBlueprintHelperWriteReviewEvidence;
class FBlueprintHelperTaskRuntimeClusterExecutorRegistry;

enum class EBlueprintHelperTaskRuntimeCluster : uint8
{
	Unknown,
	GraphWrite,
	BlueprintVariables,
	AssetFactory,
	Component,
	ClassSettings,
	Signature,
	UMGWidget,
	DataTable,
	ObjectProperty,
	MaterialInstance
};

struct FBlueprintHelperTaskRuntimeClusterExecutor
{
	EBlueprintHelperTaskRuntimeCluster Cluster = EBlueprintHelperTaskRuntimeCluster::Unknown;
	TFunction<FBlueprintHelperToolResultBase(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)> ExecuteStep;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeClusterHub
{
public:
	FBlueprintHelperTaskRuntimeClusterHub(
		const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
		const FBlueprintHelperBlueprintVariableService& InVariableService,
		const FBlueprintHelperBlueprintStructureService& InStructureService,
		const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
		const FBlueprintHelperComponentService& InComponentService,
		const FBlueprintHelperClassSettingsService& InClassSettingsService,
		const FBlueprintHelperWidgetService& InWidgetService,
		const FBlueprintHelperDataTableService& InDataTableService,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService);
	~FBlueprintHelperTaskRuntimeClusterHub();

	static bool TryLowerStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);

	static EBlueprintHelperTaskRuntimeCluster ResolveClusterForLoweredStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);
	static const TArray<EBlueprintHelperTaskRuntimeCluster>& GetRegisteredClusters();

	bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

	FBlueprintHelperToolResultBase ExecuteStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		bool bDryRun) const;

	bool BuildReviewEvidence(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence) const;

private:
	FBlueprintHelperGraphWriteTaskRuntimeCluster GraphWriteCluster;
	FBlueprintHelperBlueprintVariablesTaskRuntimeCluster BlueprintVariablesCluster;
	FBlueprintHelperSignatureTaskRuntimeCluster SignatureCluster;
	FBlueprintHelperAssetFactoryTaskRuntimeCluster AssetFactoryCluster;
	FBlueprintHelperComponentTaskRuntimeCluster ComponentCluster;
	FBlueprintHelperClassSettingsTaskRuntimeCluster ClassSettingsCluster;
	FBlueprintHelperUMGWidgetTaskRuntimeCluster UMGWidgetCluster;
	FBlueprintHelperDataTableTaskRuntimeCluster DataTableCluster;
	FBlueprintHelperObjectPropertyTaskRuntimeCluster ObjectPropertyCluster;
	FBlueprintHelperMaterialInstanceTaskRuntimeCluster MaterialInstanceCluster;
	TUniquePtr<FBlueprintHelperTaskRuntimeClusterExecutorRegistry> ClusterExecutorRegistry;
};
