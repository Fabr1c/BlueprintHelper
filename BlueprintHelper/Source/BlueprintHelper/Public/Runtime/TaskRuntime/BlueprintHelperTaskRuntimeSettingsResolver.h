// BlueprintHelper TaskRuntime settings resolver.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimePartialPreviewCacheSettings
{
	double TtlSeconds = 40.0;
	int32 MaxGroups = 64;
	int32 MaxStepEntries = 512;
	int64 MaxBytes = int64(8) * 1024 * 1024;
};

struct FBlueprintHelperTaskRuntimeCallFunctionFactCacheSettings
{
	double TtlSeconds = 180.0;
	int32 MaxEntries = 2048;
	int64 MaxBytes = int64(8) * 1024 * 1024;
};

struct FBlueprintHelperTaskRuntimeGraphWritePlanCacheSettings
{
	double TtlSeconds = 90.0;
	int32 MaxEntries = 256;
	int64 MaxBytes = int64(16) * 1024 * 1024;
};

struct FBlueprintHelperTaskRuntimeCacheSettings
{
	FBlueprintHelperTaskRuntimePartialPreviewCacheSettings PartialPreview;
	FBlueprintHelperTaskRuntimeCallFunctionFactCacheSettings CallFunctionFact;
	FBlueprintHelperTaskRuntimeGraphWritePlanCacheSettings GraphWritePlan;
	double PruneOnAccessMinIntervalSeconds = 1.0;
};

struct FBlueprintHelperTaskRuntimeExecutionPolicySettings
{
	bool bShouldCompile = false;
	bool bShouldSave = false;
	FString DryRunMode = TEXT("full");
};

struct FBlueprintHelperTaskRuntimeSettings
{
	FBlueprintHelperTaskRuntimeCacheSettings Cache;
	FBlueprintHelperTaskRuntimeExecutionPolicySettings ExecutionPolicy;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeSettingsResolver
{
public:
	static FBlueprintHelperTaskRuntimeSettings Load();
	static FBlueprintHelperTaskRuntimeCacheSettings LoadCacheSettings();
	static FBlueprintHelperTaskRuntimeExecutionPolicySettings LoadExecutionPolicy();
	static FBlueprintHelperTaskRuntimeExecutionPolicySettings ResolveExecutionPolicy(const TSharedPtr<FJsonObject>& TaskPlan);
	static TSharedRef<FJsonObject> MakeExecutionPolicyJson(const FBlueprintHelperTaskRuntimeExecutionPolicySettings& Policy);
};
