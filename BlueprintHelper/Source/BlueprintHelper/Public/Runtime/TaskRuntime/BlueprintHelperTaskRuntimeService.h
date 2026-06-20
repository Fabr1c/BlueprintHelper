// BlueprintHelper Service Layer - TaskPlan runtime executor

#pragma once

#include "CoreMinimal.h"
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
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperDebugEntryService;
class FBlueprintHelperTaskRuntimeClusterHub;
class FBlueprintHelperTaskPreviewStore;
class FBlueprintHelperTaskPartialPreviewCache;
class FBlueprintHelperTaskRuntimeCallFunctionResolutionCache;
class FBlueprintHelperGraphWritePlanCache;
class FBlueprintHelperGraphWriteCandidateArtifactStore;
/**
 * Executes BlueprintHelper.TaskPlan.v1 steps through existing capability services.
 * The first runtime cluster supports graph write TaskPlan steps backed by existing services.
 */
class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeService
{
public:
	explicit FBlueprintHelperTaskRuntimeService(
		const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
		const FBlueprintHelperBlueprintVariableService& InVariableService,
		const FBlueprintHelperBlueprintStructureService& InStructureService,
		const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
		const FBlueprintHelperComponentService& InComponentService,
		const FBlueprintHelperClassSettingsService& InClassSettingsService,
		const FBlueprintHelperWidgetService& InWidgetService,
		const FBlueprintHelperDataTableService& InDataTableService,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService,
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
		const FBlueprintHelperDebugEntryService* InDebugEntryService = nullptr);
	~FBlueprintHelperTaskRuntimeService();

	FBlueprintHelperToolResultBase PreviewTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase ExecuteTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase GetTaskRunJournal(const FString& TaskRunId) const;
	FBlueprintHelperToolResultBase GetExecutionReceipt(const FString& ReceiptId) const;

	static FBlueprintHelperValidationSummary BuildRuntimeValidation(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperValidationSummary& BaseValidation);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);

	static TSharedRef<FJsonObject> BuildTaskRunJournalForStep(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult);

	static TSharedRef<FJsonObject> BuildTaskRunJournalForSteps(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords);

private:
	void AttachPreviewToken(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperToolResultBase& Result) const;

	FBlueprintHelperToolResultBase ExecutePreviewTokenTaskPlan(
		const TSharedPtr<FJsonObject>& Payload) const;

	FBlueprintHelperToolResultBase RunTaskPlan(
		const TSharedPtr<FJsonObject>& Payload,
		bool bDryRun) const;

	TUniquePtr<FBlueprintHelperTaskRuntimeClusterHub> ClusterHub;
	TUniquePtr<FBlueprintHelperTaskPreviewStore> PreviewStore;
	TUniquePtr<FBlueprintHelperTaskPartialPreviewCache> PartialPreviewCache;
	TUniquePtr<FBlueprintHelperTaskRuntimeCallFunctionResolutionCache> CallFunctionResolutionCache;
	TUniquePtr<FBlueprintHelperGraphWritePlanCache> GraphWritePlanCache;
	TUniquePtr<FBlueprintHelperGraphWriteCandidateArtifactStore> GraphWriteCandidateArtifactStore;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr;
	mutable TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
};
