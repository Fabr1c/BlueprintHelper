#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteExecutionStatsToJsonTest,
	"BlueprintHelper.GraphWrite.ExecutionStats.ToJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteExecutionStatsToJsonTest::RunTest(const FString&)
{
	FBlueprintGraphWriteExecutionStats Stats;
	Stats.RequestedNodeCount = 3;
	Stats.SpawnedNodeCount = 3;
	Stats.RequestedDefaultValueCount = 2;
	Stats.AppliedDefaultValueCount = 2;
	Stats.RequestedLinkCount = 2;
	Stats.CreatedLinkCount = 2;
	Stats.LayoutRecordNodeCount = 3;
	Stats.SpawnNodesMs = 11.5;
	Stats.ApplyDefaultsMs = 2.25;
	Stats.ConnectLinksMs = 3.75;
	Stats.RecordLayoutMs = 1.0;

	const TSharedRef<FJsonObject> Json = FBlueprintGraphWriteExecutionStatsSerializer::ToJson(Stats);
	TestEqual(TEXT("spawned node count"), Json->GetIntegerField(TEXT("spawned_node_count")), 3);
	TestEqual(TEXT("created link count"), Json->GetIntegerField(TEXT("created_link_count")), 2);
	TestEqual(TEXT("spawn ms"), Json->GetNumberField(TEXT("spawn_nodes_ms")), 11.5);
	return true;
}

#endif
