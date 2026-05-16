// BlueprintHelper Service Layer - TaskPlan runtime executor

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperBlueprintVariableService;
class FBlueprintHelperBlueprintStructureService;
class FBlueprintHelperAssetFactoryService;
class FBlueprintHelperComponentService;
class FBlueprintHelperClassSettingsService;
class FBlueprintHelperWidgetService;
class FBlueprintHelperDataTableService;
class FBlueprintHelperPropertyReflectionService;
class FBlueprintHelperCleanupBlueprintHelperBlockService;
class FBlueprintHelperRollbackCleanupTransactionService;
class FBlueprintHelperConvertBlockToUserOwnedService;
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperDebugEntryService;
class FBlueprintHelperTaskRuntimeClusterHub;
/**
 * Executes BlueprintHelper.TaskPlan.v1 steps through existing capability services.
 * The first runtime cluster supports graph write TaskPlan steps backed by existing services.
 */
class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeService
{
public:
	explicit FBlueprintHelperTaskRuntimeService(
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
		const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
		const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
		const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
		const FBlueprintHelperBlueprintVariableService& InVariableService,
		const FBlueprintHelperBlueprintStructureService& InStructureService,
		const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
		const FBlueprintHelperComponentService& InComponentService,
		const FBlueprintHelperClassSettingsService& InClassSettingsService,
		const FBlueprintHelperWidgetService& InWidgetService,
		const FBlueprintHelperDataTableService& InDataTableService,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService,
		const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
		const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
		const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService,
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
		const FBlueprintHelperDebugEntryService* InDebugEntryService = nullptr);
	~FBlueprintHelperTaskRuntimeService();

	FBlueprintHelperToolResultBase PreviewTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase ExecuteTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase GetTaskRunJournal(const FString& TaskRunId) const;

	static FBlueprintHelperValidationSummary BuildRuntimeValidation(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperValidationSummary& BaseValidation);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);

	static TSharedRef<FJsonObject> BuildRuntimeDataForStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& TaskRunId,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		bool bDryRun);

	static TSharedRef<FJsonObject> BuildRuntimeDataForSteps(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& TaskRunId,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
		bool bDryRun);

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
	FBlueprintHelperToolResultBase RunTaskPlan(
		const TSharedPtr<FJsonObject>& Payload,
		bool bDryRun) const;

	TUniquePtr<FBlueprintHelperTaskRuntimeClusterHub> ClusterHub;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr;
	mutable TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
};
