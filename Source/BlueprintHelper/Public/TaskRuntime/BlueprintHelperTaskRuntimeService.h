// BlueprintHelper Service Layer - TaskPlan runtime executor

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

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
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperAssetBrowseService;
class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeLoweredStep
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeStepRecord
{
	FBlueprintHelperTaskRuntimeLoweredStep Step;
	FBlueprintHelperToolResultBase Result;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePostOperationRecord
{
	FString Operation;
	FBlueprintHelperToolResultBase Result;
};

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
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService);

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

	const FBlueprintHelperAppendBlueprintGraphService& AppendGraphService;
	const FBlueprintHelperReplaceBlueprintGraphService& ReplaceGraphService;
	const FBlueprintHelperPatchBlueprintGraphService& PatchGraphService;
	const FBlueprintHelperMergeBlueprintGraphService& MergeGraphService;
	const FBlueprintHelperBlueprintVariableService& VariableService;
	const FBlueprintHelperBlueprintStructureService& StructureService;
	const FBlueprintHelperAssetFactoryService& AssetFactoryService;
	const FBlueprintHelperComponentService& ComponentService;
	const FBlueprintHelperClassSettingsService& ClassSettingsService;
	const FBlueprintHelperWidgetService& WidgetService;
	const FBlueprintHelperDataTableService& DataTableService;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	mutable TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
};
