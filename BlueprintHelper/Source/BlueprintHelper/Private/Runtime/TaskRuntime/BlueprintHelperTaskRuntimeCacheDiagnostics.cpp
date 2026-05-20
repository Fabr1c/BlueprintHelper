// BlueprintHelper TaskRuntime cache diagnostics.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheDiagnostics.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeCacheDiagnostics::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("partial_preview_hits"), PartialPreviewHits);
	Json->SetNumberField(TEXT("partial_preview_misses"), PartialPreviewMisses);

	TArray<TSharedPtr<FJsonValue>> ReusedStepValues;
	for (const FString& StepId : PartialPreviewReusedSteps)
	{
		ReusedStepValues.Add(MakeShared<FJsonValueString>(StepId));
	}
	Json->SetArrayField(TEXT("partial_preview_reused_steps"), ReusedStepValues);

	Json->SetNumberField(TEXT("call_function_fact_hits"), CallFunctionFactHits);
	Json->SetNumberField(TEXT("call_function_fact_misses"), CallFunctionFactMisses);
	Json->SetNumberField(TEXT("graph_write_plan_hits"), GraphWritePlanHits);
	Json->SetNumberField(TEXT("graph_write_plan_misses"), GraphWritePlanMisses);
	Json->SetNumberField(TEXT("pruned_expired_entries"), PrunedExpiredEntries);
	Json->SetNumberField(TEXT("pruned_capacity_entries"), PrunedCapacityEntries);
	Json->SetNumberField(TEXT("current_bytes"), CurrentBytes);
	Json->SetNumberField(TEXT("partial_preview_ttl_seconds"), PartialPreviewTtlSeconds);
	Json->SetNumberField(TEXT("call_function_fact_ttl_seconds"), CallFunctionFactTtlSeconds);
	Json->SetNumberField(TEXT("graph_write_plan_ttl_seconds"), GraphWritePlanTtlSeconds);
	return Json;
}
