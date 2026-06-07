// BlueprintHelper TaskRuntime - static tool cluster hub.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterExecutorRegistry.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterFamilyRegistry.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterHubUtils.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteUnitOfWork.h"

static FString BlueprintHelperTaskRuntimeResolveRuntimeAdapterId(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor)
{
	if (Descriptor.WriteFamily.Equals(TEXT("graphwrite"), ESearchCase::IgnoreCase) &&
		!LoweredStep.AdapterOperation.IsEmpty())
	{
		return LoweredStep.AdapterOperation;
	}
	if (!LoweredStep.Capability.IsEmpty())
	{
		return LoweredStep.Capability;
	}
	if (!Descriptor.RuntimeAdapterId.IsEmpty())
	{
		return Descriptor.RuntimeAdapterId;
	}
	return Descriptor.WriteFamily;
}

static void BlueprintHelperTaskRuntimeReadAcceptedPayloadTargetFields(
	const TSharedRef<FJsonObject>& Payload,
	FBlueprintHelperAcceptedPayloadModel& Model)
{
	FString AssetPath;
	FString GraphName;
	if (Payload->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Model.TargetAssetPath = AssetPath;
	}
	if (Payload->TryGetStringField(TEXT("graph_name"), GraphName) ||
		Payload->TryGetStringField(TEXT("graph"), GraphName))
	{
		Model.GraphName = GraphName;
	}

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		if (Model.TargetAssetPath.IsEmpty() &&
			(*TargetObject)->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			Model.TargetAssetPath = AssetPath;
		}
		if (Model.GraphName.IsEmpty() &&
			((*TargetObject)->TryGetStringField(TEXT("graph_name"), GraphName) ||
			 (*TargetObject)->TryGetStringField(TEXT("graph"), GraphName)))
		{
			Model.GraphName = GraphName;
		}
	}
}

static FBlueprintHelperAcceptedPayloadModel BlueprintHelperTaskRuntimeMakeAcceptedPayloadFromLoweredStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor,
	bool bDryRun)
{
	FBlueprintHelperAcceptedPayloadModel Model;
	Model.TaskId = LoweredStep.StepId;
	Model.OperationId = LoweredStep.AdapterOperation.IsEmpty()
		? LoweredStep.RuntimeOperation
		: LoweredStep.AdapterOperation;
	Model.WriteFamily = Descriptor.WriteFamily;
	Model.RuntimeAdapterId = BlueprintHelperTaskRuntimeResolveRuntimeAdapterId(LoweredStep, Descriptor);
	Model.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
	Model.BridgeCommand = Descriptor.BridgeCommand;
	Model.Mode = bDryRun ? TEXT("preview") : TEXT("execute");
	Model.SourcePayloadSchema = TEXT("BlueprintHelper.TaskRuntimeLoweredStep.v1");
	Model.RawPayload = LoweredStep.Payload;

	if (LoweredStep.Payload.IsValid())
	{
		BlueprintHelperTaskRuntimeReadAcceptedPayloadTargetFields(LoweredStep.Payload.ToSharedRef(), Model);
	}
	Model.ReviewScopeIdentity = FBlueprintHelperAcceptedPayloadModelUtils::MakeReviewScopeIdentity(Model);
	Model.DebugTraceId = FBlueprintHelperAcceptedPayloadModelUtils::MakeDebugTraceId(Model);
	return Model;
}

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
	ClusterExecutorRegistry = FBlueprintHelperTaskRuntimeClusterExecutorRegistry::CreateDefault(
		GraphWriteCluster,
		BlueprintVariablesCluster,
		AssetFactoryCluster,
		ComponentCluster,
		ClassSettingsCluster,
		SignatureCluster,
		UMGWidgetCluster,
		DataTableCluster,
		ObjectPropertyCluster);
}

FBlueprintHelperTaskRuntimeClusterHub::~FBlueprintHelperTaskRuntimeClusterHub() = default;

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
	static const TArray<EBlueprintHelperTaskRuntimeCluster> RegisteredClusters =
		FBlueprintHelperTaskRuntimeClusterFamilyRegistry::GetRegisteredClusters();
	return RegisteredClusters;
}

bool FBlueprintHelperTaskRuntimeClusterHub::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor ClusterDescriptor;
	const EBlueprintHelperTaskRuntimeCluster Cluster = ResolveClusterForLoweredStep(LoweredStep);
	return FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByCluster(Cluster, ClusterDescriptor) &&
		ClusterExecutorRegistry.IsValid() &&
		ClusterExecutorRegistry->CanExecuteCluster(Cluster);
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
	FBlueprintHelperTaskRuntimeClusterFamilyDescriptor ClusterDescriptor;
	const FBlueprintHelperTaskRuntimeClusterExecutor* Executor = ClusterExecutorRegistry.IsValid()
		? ClusterExecutorRegistry->FindByCluster(Cluster)
		: nullptr;
	if (!Executor ||
		!FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByCluster(Cluster, ClusterDescriptor))
	{
		return FBlueprintHelperTaskRuntimeClusterHubUtils::MakeUnsupportedAdapterOperationResult(LoweredStep);
	}

	FBlueprintHelperWriteFamilyDescriptor WriteDescriptor;
	if (!FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByWriteFamily(
		ClusterDescriptor.WriteFamily,
		WriteDescriptor))
	{
		return FBlueprintHelperTaskRuntimeClusterHubUtils::MakeUnsupportedAdapterOperationResult(LoweredStep);
	}

	TSharedPtr<IBlueprintHelperWriteFamilyAdapter> Adapter;
	if (!FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByWriteFamily(
		WriteDescriptor.WriteFamily,
		Adapter) ||
		!Adapter.IsValid())
	{
		return FBlueprintHelperTaskRuntimeClusterHubUtils::MakeUnsupportedAdapterOperationResult(LoweredStep);
	}

	const FBlueprintHelperAcceptedPayloadModel AcceptedPayload =
		BlueprintHelperTaskRuntimeMakeAcceptedPayloadFromLoweredStep(
			LoweredStep,
			WriteDescriptor,
			bDryRun);

	FBlueprintHelperWriteUnitOfWorkRequest Request;
	FBlueprintHelperToolError Error;
	if (!Adapter->BuildPreflight(AcceptedPayload, Request, Error))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	Request.Mode = bDryRun
		? EBlueprintHelperWriteUnitOfWorkMode::Preview
		: EBlueprintHelperWriteUnitOfWorkMode::Execute;
	Request.ApplyMutation = [Executor, &LoweredStep]()
	{
		return Executor->ExecuteStep(LoweredStep);
	};
	if (!Adapter->BuildMutationPlan(AcceptedPayload, Request, Error) ||
		!Adapter->BuildDryRunProjection(AcceptedPayload, Request, Error) ||
		!Adapter->BuildReviewAndReadback(AcceptedPayload, Request, Error))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	return FBlueprintHelperWriteUnitOfWork::Run(Request).ToolResult;
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
	return FBlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry::TryBuild(
		Cluster,
		LoweredStep,
		StepResult,
		ArchiveSessionId,
		TaskRunId,
		StepIndex,
		OutEvidence);
}
