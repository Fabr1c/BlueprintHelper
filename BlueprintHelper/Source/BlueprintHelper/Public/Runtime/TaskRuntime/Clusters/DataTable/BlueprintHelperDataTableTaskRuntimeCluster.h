// BlueprintHelper TaskRuntime - DataTable static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperDataTableService;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperDataTableTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperDataTableTaskRuntimeCluster(
		const FBlueprintHelperDataTableService& InDataTableService);

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
	const FBlueprintHelperDataTableService& DataTableService;
};
