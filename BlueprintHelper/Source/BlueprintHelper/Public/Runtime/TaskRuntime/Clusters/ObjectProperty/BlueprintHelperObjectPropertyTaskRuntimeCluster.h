// BlueprintHelper TaskRuntime - ObjectProperty static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperPropertyReflectionService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperObjectPropertyTaskRuntimeCluster(
		const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	static bool BuildReviewEvidence(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperPropertyReflectionService& PropertyReflectionService;
};
