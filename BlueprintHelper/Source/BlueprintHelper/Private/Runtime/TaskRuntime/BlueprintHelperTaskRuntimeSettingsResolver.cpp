// BlueprintHelper TaskRuntime settings resolver implementation.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeSettingsResolver.h"

#include "Dom/JsonObject.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

namespace
{
	static FString BlueprintHelperNormalizeDryRunMode(FString Mode)
	{
		Mode.TrimStartAndEndInline();
		Mode = Mode.ToLower();
		if (Mode == TEXT("quick") || Mode == TEXT("none"))
		{
			return Mode;
		}
		return TEXT("full");
	}
}

FBlueprintHelperTaskRuntimeSettings FBlueprintHelperTaskRuntimeSettingsResolver::Load()
{
	FBlueprintHelperTaskRuntimeSettings Settings;
	Settings.Cache = LoadCacheSettings();
	Settings.ExecutionPolicy = LoadExecutionPolicy();
	return Settings;
}

FBlueprintHelperTaskRuntimeCacheSettings FBlueprintHelperTaskRuntimeSettingsResolver::LoadCacheSettings()
{
	FBlueprintHelperTaskRuntimeCacheSettings Settings;

	Settings.PartialPreview.TtlSeconds = FMath::Max(
		0.0,
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("runtime.task_runtime.cache.partial_preview.ttl_seconds"), Settings.PartialPreview.TtlSeconds));
	Settings.PartialPreview.MaxGroups = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.partial_preview.max_groups"), Settings.PartialPreview.MaxGroups));
	Settings.PartialPreview.MaxStepEntries = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.partial_preview.max_step_entries"), Settings.PartialPreview.MaxStepEntries));
	Settings.PartialPreview.MaxBytes = FMath::Max<int64>(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.partial_preview.max_bytes"), static_cast<int32>(Settings.PartialPreview.MaxBytes)));

	Settings.CallFunctionFact.TtlSeconds = FMath::Max(
		0.0,
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("runtime.task_runtime.cache.call_function_fact.ttl_seconds"), Settings.CallFunctionFact.TtlSeconds));
	Settings.CallFunctionFact.MaxEntries = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.call_function_fact.max_entries"), Settings.CallFunctionFact.MaxEntries));
	Settings.CallFunctionFact.MaxBytes = FMath::Max<int64>(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.call_function_fact.max_bytes"), static_cast<int32>(Settings.CallFunctionFact.MaxBytes)));

	Settings.GraphWritePlan.TtlSeconds = FMath::Max(
		0.0,
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("runtime.task_runtime.cache.graph_write_plan.ttl_seconds"), Settings.GraphWritePlan.TtlSeconds));
	Settings.GraphWritePlan.MaxEntries = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.graph_write_plan.max_entries"), Settings.GraphWritePlan.MaxEntries));
	Settings.GraphWritePlan.MaxBytes = FMath::Max<int64>(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.task_runtime.cache.graph_write_plan.max_bytes"), static_cast<int32>(Settings.GraphWritePlan.MaxBytes)));

	Settings.PruneOnAccessMinIntervalSeconds = FMath::Max(
		0.0,
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("runtime.task_runtime.cache.prune_on_access_min_interval_seconds"), Settings.PruneOnAccessMinIntervalSeconds));
	return Settings;
}

FBlueprintHelperTaskRuntimeExecutionPolicySettings FBlueprintHelperTaskRuntimeSettingsResolver::LoadExecutionPolicy()
{
	FBlueprintHelperTaskRuntimeExecutionPolicySettings Settings;
	Settings.bShouldCompile = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("runtime.task_runtime.execution_policy.should_compile"),
		Settings.bShouldCompile);
	Settings.bShouldSave = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("runtime.task_runtime.execution_policy.should_save"),
		Settings.bShouldSave);
	Settings.DryRunMode = BlueprintHelperNormalizeDryRunMode(FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("runtime.task_runtime.execution_policy.dry_run_mode"),
		Settings.DryRunMode));
	return Settings;
}

FBlueprintHelperTaskRuntimeExecutionPolicySettings FBlueprintHelperTaskRuntimeSettingsResolver::ResolveExecutionPolicy(
	const TSharedPtr<FJsonObject>& TaskPlan)
{
	FBlueprintHelperTaskRuntimeExecutionPolicySettings Policy = LoadExecutionPolicy();

	const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
	if (!TaskPlan.IsValid() ||
		!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
		!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
	{
		return Policy;
	}

	bool bBoolValue = false;
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bBoolValue))
	{
		Policy.bShouldCompile = bBoolValue;
	}
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bBoolValue))
	{
		Policy.bShouldSave = bBoolValue;
	}

	FString DryRunMode;
	if ((*ExecutionPolicyPtr)->TryGetStringField(TEXT("dry_run_mode"), DryRunMode))
	{
		Policy.DryRunMode = BlueprintHelperNormalizeDryRunMode(DryRunMode);
	}
	return Policy;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeSettingsResolver::MakeExecutionPolicyJson(
	const FBlueprintHelperTaskRuntimeExecutionPolicySettings& Policy)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("should_compile"), Policy.bShouldCompile);
	Json->SetBoolField(TEXT("should_save"), Policy.bShouldSave);
	Json->SetStringField(TEXT("dry_run_mode"), BlueprintHelperNormalizeDryRunMode(Policy.DryRunMode));
	return Json;
}
