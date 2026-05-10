// BlueprintHelper TaskRuntime - UMGWidget static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperWidgetService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperUMGWidgetTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperUMGWidgetTaskRuntimeCluster(
		const FBlueprintHelperWidgetService& InWidgetService);

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
	const FBlueprintHelperWidgetService& WidgetService;
};
