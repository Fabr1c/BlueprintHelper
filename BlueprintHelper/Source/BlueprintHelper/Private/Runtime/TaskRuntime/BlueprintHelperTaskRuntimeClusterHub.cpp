// BlueprintHelper TaskRuntime - static tool cluster hub.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterHubUtils.h"

FBlueprintHelperTaskRuntimeClusterHub::FBlueprintHelperTaskRuntimeClusterHub(
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperBlueprintStructureService& InStructureService,
	const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettingsService,
	const FBlueprintHelperWidgetService& InWidgetService,
	const FBlueprintHelperDataTableService& InDataTableService,
	const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService)
	: GraphWriteCluster(InGraphWriteRegistry)
	, BlueprintVariablesCluster(InVariableService)
	, SignatureCluster(InStructureService)
	, AssetFactoryCluster(InAssetFactoryService)
	, ComponentCluster(InComponentService)
	, ClassSettingsCluster(InClassSettingsService)
	, UMGWidgetCluster(InWidgetService)
	, DataTableCluster(InDataTableService)
	, ObjectPropertyCluster(InPropertyReflectionService)
{
}

bool FBlueprintHelperTaskRuntimeClusterHub::TryLowerStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	return FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		StepObject,
		bDryRun,
		OutLoweredStep,
		OutError);
}

EBlueprintHelperTaskRuntimeCluster FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return FBlueprintHelperTaskRuntimeClusterHubUtils::ResolveClusterForLoweredStep(LoweredStep);
}

const TArray<EBlueprintHelperTaskRuntimeCluster>& FBlueprintHelperTaskRuntimeClusterHub::GetRegisteredClusters()
{
	static const TArray<EBlueprintHelperTaskRuntimeCluster> RegisteredClusters = {
		EBlueprintHelperTaskRuntimeCluster::GraphWrite,
		EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
		EBlueprintHelperTaskRuntimeCluster::AssetFactory,
		EBlueprintHelperTaskRuntimeCluster::Component,
		EBlueprintHelperTaskRuntimeCluster::ClassSettings,
		EBlueprintHelperTaskRuntimeCluster::Signature,
		EBlueprintHelperTaskRuntimeCluster::UMGWidget,
		EBlueprintHelperTaskRuntimeCluster::DataTable,
		EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
	};
	return RegisteredClusters;
}

bool FBlueprintHelperTaskRuntimeClusterHub::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return GetRegisteredClusters().Contains(ResolveClusterForLoweredStep(LoweredStep));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterHub::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	bool bDryRun) const
{
	if (bDryRun && !LoweredStep.bAdapterDryRunSupported)
	{
		FBlueprintHelperToolResultBase StepResult = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		StepResult.Data = FBlueprintHelperTaskRuntimeClusterHubUtils::MakeSyntheticDryRunData();
		return StepResult;
	}

	const EBlueprintHelperTaskRuntimeCluster Cluster = ResolveClusterForLoweredStep(LoweredStep);
	switch (Cluster)
	{
	case EBlueprintHelperTaskRuntimeCluster::GraphWrite:
		return GraphWriteCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::BlueprintVariables:
		return BlueprintVariablesCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::AssetFactory:
		return AssetFactoryCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::Component:
		return ComponentCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::ClassSettings:
		return ClassSettingsCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::Signature:
		return SignatureCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::UMGWidget:
		return UMGWidgetCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::DataTable:
		return DataTableCluster.ExecuteStep(LoweredStep);
	case EBlueprintHelperTaskRuntimeCluster::ObjectProperty:
		return ObjectPropertyCluster.ExecuteStep(LoweredStep);
	default:
		break;
	}

	return FBlueprintHelperTaskRuntimeClusterHubUtils::MakeUnsupportedAdapterOperationResult(LoweredStep);
}

bool FBlueprintHelperTaskRuntimeClusterHub::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence) const
{
	const EBlueprintHelperTaskRuntimeCluster Cluster = ResolveClusterForLoweredStep(LoweredStep);
	switch (Cluster)
	{
	case EBlueprintHelperTaskRuntimeCluster::GraphWrite:
		return FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::BlueprintVariables:
		return FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::AssetFactory:
		return FBlueprintHelperAssetFactoryTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::Component:
		return FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::ClassSettings:
		return FBlueprintHelperClassSettingsTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::Signature:
		return FBlueprintHelperSignatureTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::UMGWidget:
		return FBlueprintHelperUMGWidgetTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::DataTable:
		return FBlueprintHelperDataTableTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	case EBlueprintHelperTaskRuntimeCluster::ObjectProperty:
		return FBlueprintHelperObjectPropertyTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	default:
		break;
	}

	return false;
}
