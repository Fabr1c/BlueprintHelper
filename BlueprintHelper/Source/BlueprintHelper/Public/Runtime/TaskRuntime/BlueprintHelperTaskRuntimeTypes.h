// BlueprintHelper TaskRuntime shared DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeLoweredStep
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeStepRecord
{
	FBlueprintHelperTaskRuntimeLoweredStep Step;
	FBlueprintHelperToolResultBase Result;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePostOperationRecord
{
	FString Operation;
	FBlueprintHelperToolResultBase Result;
	FString AssetPath;
	FString Status;
	FString Reason;
	double DurationMs = 0.0;
};
