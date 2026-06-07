#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimeStage.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;
class FBlueprintHelperTaskRuntimeClusterHub;
class FBlueprintHelperTaskRuntimePipelineRunner;
struct FBlueprintHelperWriteReviewEvidence;

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineStageTrace
{
	EBlueprintHelperTaskRuntimePipelineStage Stage =
		EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract;
	FString Name;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineExecutionContext
{
	TSharedPtr<FJsonObject> TaskPlan;
	FString TaskRunId;
	bool bDryRun = false;
	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PostOperationRecords;
};

class BLUEPRINTHELPER_API IBlueprintHelperTaskRuntimePipelineStageExecutor
{
public:
	virtual ~IBlueprintHelperTaskRuntimePipelineStageExecutor() = default;
	virtual EBlueprintHelperTaskRuntimePipelineStage GetStage() const = 0;
	virtual FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context) = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineContext
{
public:
	void RecordStage(EBlueprintHelperTaskRuntimePipelineStage Stage);
	bool HasStage(EBlueprintHelperTaskRuntimePipelineStage Stage) const;
	const TArray<FBlueprintHelperTaskRuntimePipelineStageTrace>& GetStageTrace() const;

	TSharedPtr<FJsonObject> TaskPlan;
	TArray<FBlueprintHelperTaskRuntimeLoweredStep> LoweredSteps;
	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	FBlueprintHelperToolResultBase RuntimeResult;
	FString TaskRunId;
	bool bDryRun = false;

private:
	TArray<FBlueprintHelperTaskRuntimePipelineStageTrace> StageTrace;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipeline
{
public:
	FBlueprintHelperTaskRuntimePipeline() = default;
	explicit FBlueprintHelperTaskRuntimePipeline(
		TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> InExecutors);
	FBlueprintHelperTaskRuntimePipeline(const FBlueprintHelperTaskRuntimePipeline&) = delete;
	FBlueprintHelperTaskRuntimePipeline& operator=(const FBlueprintHelperTaskRuntimePipeline&) = delete;
	FBlueprintHelperTaskRuntimePipeline(FBlueprintHelperTaskRuntimePipeline&&) = default;
	FBlueprintHelperTaskRuntimePipeline& operator=(FBlueprintHelperTaskRuntimePipeline&&) = default;

	FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineRunner& Runner,
		FBlueprintHelperTaskRuntimePipelineExecutionContext& ExecutionContext);

	static TArray<EBlueprintHelperTaskRuntimePipelineStage> GetDefaultStageOrder();
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

private:
	TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> Executors;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineRunner
{
public:
	FBlueprintHelperTaskRuntimePipelineRunner(
		const TSharedPtr<FJsonObject>& InTaskPlan,
		const FString& InTaskRunId,
		bool bInDryRun);

	FBlueprintHelperTaskRuntimePipelineExecutionContext MakeExecutionContext() const;
	FBlueprintHelperToolResultBase ExecuteStage(
		IBlueprintHelperTaskRuntimePipelineStageExecutor& Executor,
		FBlueprintHelperTaskRuntimePipelineExecutionContext& ExecutionContext);
	void RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage Stage);
	bool HasStage(EBlueprintHelperTaskRuntimePipelineStage Stage) const;
	void SetStepRecords(const TArray<FBlueprintHelperTaskRuntimeStepRecord>& InStepRecords);
	bool BuildReviewEvidence(
		const FBlueprintHelperTaskRuntimeClusterHub& ClusterHub,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	TSharedRef<FJsonObject> BuildRuntimeData(
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& InStepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& InPostOperationRecords);
	void AttachToResult(FBlueprintHelperToolResultBase& RuntimeResult) const;
	const FBlueprintHelperTaskRuntimePipelineContext& GetContext() const;

private:
	FBlueprintHelperTaskRuntimePipelineContext Context;
};
