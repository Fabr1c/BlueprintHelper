// BlueprintHelper TaskRuntime cache configuration.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperTaskRuntimeCacheConfig
{
	FTimespan PartialPreviewTtl;
	int32 PartialPreviewMaxGroups = 0;
	int32 PartialPreviewMaxStepEntries = 0;
	int64 PartialPreviewMaxBytes = 0;

	FTimespan CallFunctionFactTtl;
	int32 CallFunctionFactMaxEntries = 0;
	int64 CallFunctionFactMaxBytes = 0;

	FTimespan GraphWritePlanTtl;
	int32 GraphWritePlanMaxEntries = 0;
	int64 GraphWritePlanMaxBytes = 0;

	FTimespan PruneOnAccessMinInterval;

	static FBlueprintHelperTaskRuntimeCacheConfig Default();
};
