// BlueprintHelper TaskRuntime prepared run DTO.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePreparedStep.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimePreparedTaskRun
{
	TSharedPtr<FJsonObject> TaskPlan;
	TArray<FBlueprintHelperTaskRuntimePreparedStep> Steps;
	TArray<FString> TargetAssets;
	FBlueprintHelperTaskRuntimeDryRunPolicy DryRunPolicy;
	FString TaskRunId;
	FString ArchiveSessionId;
	bool bDryRun = false;
	bool bQuickDryRun = false;
};
