#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationExecutor.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeTimingUtils.h"

class FBlueprintHelperTaskRuntimeCommitService;

class FBlueprintHelperTaskRuntimeCallbackStageExecutor final
	: public IBlueprintHelperTaskRuntimePipelineStageExecutor
{
public:
	using FCallback = TFunction<FBlueprintHelperToolResultBase(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)>;

	FBlueprintHelperTaskRuntimeCallbackStageExecutor(
		EBlueprintHelperTaskRuntimePipelineStage InStage,
		FCallback InCallback);

	virtual EBlueprintHelperTaskRuntimePipelineStage GetStage() const override;
	virtual FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context) override;

private:
	EBlueprintHelperTaskRuntimePipelineStage Stage;
	FCallback Callback;
};

class FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor final
	: public IBlueprintHelperTaskRuntimePipelineStageExecutor
{
public:
	FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor(
		const FBlueprintHelperTaskRuntimeCommitService& InCommitService,
		const FBlueprintHelperTaskRuntimeLoweredStep& InLoweredStep,
		bool bInDryRun);

	virtual EBlueprintHelperTaskRuntimePipelineStage GetStage() const override;
	virtual FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context) override;

private:
	const FBlueprintHelperTaskRuntimeCommitService& CommitService;
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep;
	bool bDryRun = false;
};

class FBlueprintHelperTaskRuntimePostOperationStageExecutor final
	: public IBlueprintHelperTaskRuntimePipelineStageExecutor
{
public:
	FBlueprintHelperTaskRuntimePostOperationStageExecutor(
		const FBlueprintHelperTaskRuntimePostOperationPlan& InPlan,
		EBlueprintHelperTaskRuntimePostOperationKind InKind,
		const FBlueprintHelperTaskRuntimeCommitService* InCommitService);

	virtual EBlueprintHelperTaskRuntimePipelineStage GetStage() const override;
	virtual FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context) override;

	const FBlueprintHelperTaskRuntimePostOperationExecutionResult& GetExecutionResult() const;

private:
	static FBlueprintHelperTaskRuntimePostOperationPlan FilterPlanByKind(
		const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
		EBlueprintHelperTaskRuntimePostOperationKind Kind);
	static FBlueprintHelperTaskRuntimePostOperationRecord ConvertRecord(
		const FBlueprintHelperTaskRuntimePostOperationRecordEx& Record);

	FBlueprintHelperTaskRuntimePostOperationPlan Plan;
	EBlueprintHelperTaskRuntimePostOperationKind Kind =
		EBlueprintHelperTaskRuntimePostOperationKind::Compile;
	const FBlueprintHelperTaskRuntimeCommitService* CommitService = nullptr;
	FBlueprintHelperTaskRuntimePostOperationExecutionResult ExecutionResult;
};

class FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor final
	: public IBlueprintHelperTaskRuntimePipelineStageExecutor
{
public:
	FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor(
		const FBlueprintHelperTaskRuntimeCommitService* InCommitService,
		FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace* InTimingTrace);

	virtual EBlueprintHelperTaskRuntimePipelineStage GetStage() const override;
	virtual FBlueprintHelperToolResultBase Execute(
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context) override;

	const FBlueprintHelperTaskRuntimePostOperationExecutionResult& GetLastExecutionResult() const;

private:
	FBlueprintHelperTaskRuntimePostOperationExecutionResult ExecuteKind(
		const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
		EBlueprintHelperTaskRuntimePostOperationKind Kind,
		const FString& TimingName,
		FBlueprintHelperTaskRuntimePipelineExecutionContext& Context);
	static FBlueprintHelperToolResultBase MakePostOperationFailure(
		const FBlueprintHelperTaskRuntimePostOperationExecutionResult& ExecutionResult);

	const FBlueprintHelperTaskRuntimeCommitService* CommitService = nullptr;
	FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace* TimingTrace = nullptr;
	FBlueprintHelperTaskRuntimePostOperationExecutionResult LastExecutionResult;
};
