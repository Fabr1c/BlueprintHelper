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
	return Settings;
}
