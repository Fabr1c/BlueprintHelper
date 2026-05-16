// BlueprintHelper TaskRuntime - ClassSettings static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperClassSettingsService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperClassSettingsTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperClassSettingsTaskRuntimeCluster(
		const FBlueprintHelperClassSettingsService& InClassSettingsService);

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
	const FBlueprintHelperClassSettingsService& ClassSettingsService;
};
