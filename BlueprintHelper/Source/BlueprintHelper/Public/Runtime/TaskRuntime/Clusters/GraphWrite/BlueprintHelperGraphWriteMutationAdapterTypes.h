// BlueprintHelper GraphWrite task-runtime mutation adapter types.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteLoweringRequest
{
	TSharedPtr<FJsonObject> TaskPlan;
	TSharedPtr<FJsonObject> StepObject;
	bool bDryRun = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteLoweringResult
{
	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
};
