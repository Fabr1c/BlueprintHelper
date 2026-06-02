#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"

#include "Dom/JsonObject.h"

TSharedRef<FJsonObject> FBlueprintGraphWriteExecutionStatsSerializer::ToJson(
	const FBlueprintGraphWriteExecutionStats& Stats)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("requested_node_count"), Stats.RequestedNodeCount);
	Json->SetNumberField(TEXT("spawned_node_count"), Stats.SpawnedNodeCount);
	Json->SetNumberField(TEXT("requested_default_value_count"), Stats.RequestedDefaultValueCount);
	Json->SetNumberField(TEXT("applied_default_value_count"), Stats.AppliedDefaultValueCount);
	Json->SetNumberField(TEXT("requested_link_count"), Stats.RequestedLinkCount);
	Json->SetNumberField(TEXT("created_link_count"), Stats.CreatedLinkCount);
	Json->SetNumberField(TEXT("connectivity_violation_count"), Stats.ConnectivityViolationCount);
	Json->SetNumberField(TEXT("layout_record_node_count"), Stats.LayoutRecordNodeCount);
	Json->SetNumberField(TEXT("build_context_ms"), Stats.BuildContextMs);
	Json->SetNumberField(TEXT("build_plan_ms"), Stats.BuildPlanMs);
	Json->SetNumberField(TEXT("spawn_nodes_ms"), Stats.SpawnNodesMs);
	Json->SetNumberField(TEXT("apply_defaults_ms"), Stats.ApplyDefaultsMs);
	Json->SetNumberField(TEXT("connect_links_ms"), Stats.ConnectLinksMs);
	Json->SetNumberField(TEXT("connectivity_validation_ms"), Stats.ConnectivityValidationMs);
	Json->SetNumberField(TEXT("record_layout_ms"), Stats.RecordLayoutMs);
	return Json;
}
