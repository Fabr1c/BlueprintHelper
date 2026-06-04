#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionSettingsResolver.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

FBlueprintHelperActionResolutionSettings FBlueprintHelperActionResolutionSettingsResolver::Load()
{
	FBlueprintHelperActionResolutionSettings Settings;
	Settings.CandidateCount = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("tool_clusters.graph_write.action_resolution.max_candidates"),
			8));
	Settings.DefaultSearchMode = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("tool_clusters.graph_write.action_resolution.default_search_mode"),
		FString());
	Settings.DefaultAmbiguityPolicy = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("tool_clusters.graph_write.action_resolution.default_ambiguity_policy"),
		FString());
	Settings.AutoSearchMaxCandidatesPerStatement = FMath::Clamp(
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_candidates_per_statement"),
			3),
		1,
		10);
	Settings.AutoSearchMaxStatements = FMath::Clamp(
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_statements"),
			16),
		1,
		64);
	Settings.AutoSearchMaxTotalMs = FMath::Clamp(
		FBlueprintHelperRuntimeSettingResolver::GetInt(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_total_ms"),
			120),
		1,
		1000);
	Settings.AutoSearchDetailLevel = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("tool_clusters.graph_write.action_resolution.auto_search.detail_level"),
		TEXT("short"));
	if (!Settings.AutoSearchDetailLevel.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase))
	{
		Settings.AutoSearchDetailLevel = TEXT("short");
	}
	else
	{
		Settings.AutoSearchDetailLevel = TEXT("diagnostic");
	}
	return Settings;
}
