#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimeStage.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineStageTrace
{
	EBlueprintHelperTaskRuntimePipelineStage Stage =
		EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract;
	FString Name;
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
	static TArray<EBlueprintHelperTaskRuntimePipelineStage> GetDefaultStageOrder();
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineRunner
{
public:
	FBlueprintHelperTaskRuntimePipelineRunner(
		const TSharedPtr<FJsonObject>& InTaskPlan,
		const FString& InTaskRunId,
		bool bInDryRun);

	void RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage Stage);
	bool HasStage(EBlueprintHelperTaskRuntimePipelineStage Stage) const;
	void SetStepRecords(const TArray<FBlueprintHelperTaskRuntimeStepRecord>& InStepRecords);
	void AttachToResult(FBlueprintHelperToolResultBase& RuntimeResult) const;
	const FBlueprintHelperTaskRuntimePipelineContext& GetContext() const;

private:
	FBlueprintHelperTaskRuntimePipelineContext Context;
};
