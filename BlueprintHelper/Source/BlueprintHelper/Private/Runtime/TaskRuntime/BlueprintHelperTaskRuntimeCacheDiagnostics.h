// BlueprintHelper TaskRuntime cache diagnostics.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimeCacheDiagnostics
{
	int32 PartialPreviewHits = 0;
	int32 PartialPreviewMisses = 0;
	TArray<FString> PartialPreviewReusedSteps;

	int32 CallFunctionFactHits = 0;
	int32 CallFunctionFactMisses = 0;

	int32 GraphWritePlanHits = 0;
	int32 GraphWritePlanMisses = 0;

	int32 PrunedExpiredEntries = 0;
	int32 PrunedCapacityEntries = 0;
	int64 CurrentBytes = 0;

	double PartialPreviewTtlSeconds = 0.0;
	double CallFunctionFactTtlSeconds = 0.0;
	double GraphWritePlanTtlSeconds = 0.0;

	TSharedRef<FJsonObject> ToJson() const;
};
