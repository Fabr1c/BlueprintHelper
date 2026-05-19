// BlueprintHelper TaskRuntime pure prepare service.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePreparedTaskRun.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class FBlueprintHelperTaskRuntimePrepareService
{
public:
	bool Prepare(
		const TSharedPtr<FJsonObject>& Payload,
		bool bDryRun,
		FBlueprintHelperTaskRuntimePreparedTaskRun& OutPreparedRun,
		FBlueprintHelperToolError& OutError) const;

	static FString GetTaskPlanStepId(
		const TSharedPtr<FJsonObject>& StepObject,
		int32 StepIndex);

	static TArray<FString> ReadStepDependsOn(
		const TSharedPtr<FJsonObject>& StepObject);

	static TArray<FString> ReadTargetAssets(
		const TSharedPtr<FJsonObject>& TaskPlan);

private:
	static FBlueprintHelperToolError MakeTaskRuntimeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""));

	static void NormalizeErrorField(
		FBlueprintHelperToolError& Error,
		int32 StepIndex);
};
