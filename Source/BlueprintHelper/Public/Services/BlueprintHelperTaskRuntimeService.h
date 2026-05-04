// BlueprintHelper Service Layer - TaskPlan runtime executor

#pragma once

#include "CoreMinimal.h"
#include "Services/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FJsonObject;

/**
 * Executes BlueprintHelper.TaskPlan.v1 steps through existing capability services.
 * First slice supports a single append_blueprint_graph step.
 */
class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeService
{
public:
	explicit FBlueprintHelperTaskRuntimeService(
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService);

	FBlueprintHelperToolResultBase PreviewTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase ExecuteTaskPlan(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase GetTaskRunJournal(const FString& TaskRunId) const;

	static FBlueprintHelperValidationSummary BuildRuntimeValidation(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperValidationSummary& BaseValidation);

private:
	FBlueprintHelperToolResultBase RunTaskPlan(
		const TSharedPtr<FJsonObject>& Payload,
		bool bDryRun) const;

	const FBlueprintHelperAppendBlueprintGraphService& AppendGraphService;
	mutable TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
};
