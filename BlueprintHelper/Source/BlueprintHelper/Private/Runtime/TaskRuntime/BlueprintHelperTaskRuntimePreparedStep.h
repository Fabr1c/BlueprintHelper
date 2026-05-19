// BlueprintHelper TaskRuntime prepared step DTO.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimePreparedStep
{
	int32 StepIndex = INDEX_NONE;
	FString StepId;
	TArray<FString> DependsOn;
	TSharedPtr<FJsonObject> StepObject;
	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
};
