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

bool FBlueprintHelperTaskRuntimeClusterHub::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return ResolveClusterForLoweredStep(LoweredStep) != EBlueprintHelperTaskRuntimeCluster::Unknown;
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

	using FExecuteStepHandler = TFunction<FBlueprintHelperToolResultBase(const FBlueprintHelperTaskRuntimeLoweredStep&)>;

	TMap<EBlueprintHelperTaskRuntimeCluster, FExecuteStepHandler> ExecuteHandlers;
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::GraphWrite,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return GraphWriteCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return BlueprintVariablesCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::AssetFactory,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return AssetFactoryCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::Component,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return ComponentCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::ClassSettings,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return ClassSettingsCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::Signature,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return SignatureCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::UMGWidget,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return UMGWidgetCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::DataTable,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return DataTableCluster.ExecuteStep(Step);
		});
	ExecuteHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
		[this](const FBlueprintHelperTaskRuntimeLoweredStep& Step)
		{
			return ObjectPropertyCluster.ExecuteStep(Step);
		});
	const EBlueprintHelperTaskRuntimeCluster Cluster = ResolveClusterForLoweredStep(LoweredStep);
	if (const FExecuteStepHandler* ExecuteHandler = ExecuteHandlers.Find(Cluster))
	{
		return (*ExecuteHandler)(LoweredStep);
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
	using FBuildReviewEvidenceHandler = TFunction<bool(
		const FBlueprintHelperTaskRuntimeLoweredStep&,
		const FBlueprintHelperToolResultBase&,
		const FString&,
		const FString&,
		int32,
		FBlueprintHelperWriteReviewEvidence&)>;

	TMap<EBlueprintHelperTaskRuntimeCluster, FBuildReviewEvidenceHandler> EvidenceHandlers;
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::GraphWrite,
		&FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
		&FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::AssetFactory,
		&FBlueprintHelperAssetFactoryTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::Component,
		&FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::ClassSettings,
		&FBlueprintHelperClassSettingsTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::Signature,
		&FBlueprintHelperSignatureTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::UMGWidget,
		&FBlueprintHelperUMGWidgetTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::DataTable,
		&FBlueprintHelperDataTableTaskRuntimeCluster::BuildReviewEvidence);
	EvidenceHandlers.Add(
		EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
		&FBlueprintHelperObjectPropertyTaskRuntimeCluster::BuildReviewEvidence);
	const EBlueprintHelperTaskRuntimeCluster Cluster = ResolveClusterForLoweredStep(LoweredStep);
	if (const FBuildReviewEvidenceHandler* EvidenceHandler = EvidenceHandlers.Find(Cluster))
	{
		return (*EvidenceHandler)(
			LoweredStep,
			StepResult,
			ArchiveSessionId,
			TaskRunId,
			StepIndex,
			OutEvidence);
	}

	return false;
}
