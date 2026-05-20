#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphMutationPlanBuilderExplicitJsonTest,
	"BlueprintHelper.GraphWrite.MutationPlanBuilder.ExplicitJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphMutationPlanBuilderExplicitJsonTest::RunTest(const FString&)
{
	const FString JsonText = TEXT(R"({"graph_name":"EventGraph","nodes":[{"id":"print_001","type":"CallFunction","function":"PrintString","defaults":{"InString":"Hello"}}],"links":[{"from":"entry","from_pin":"then","to":"print_001","to_pin":"execute"}]})");
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TestTrue(TEXT("json parse"), FJsonSerializer::Deserialize(Reader, Json));

	FBlueprintGraphMutationPlan Plan;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
	TestTrue(TEXT("build plan"), FBlueprintGraphMutationPlanBuilder::BuildFromGraphJson(Json, Plan, Diagnostics));
	TestEqual(TEXT("nodes"), Plan.CountRequestedNodes(), 1);
	TestEqual(TEXT("defaults"), Plan.CountRequestedDefaultValues(), 1);
	TestEqual(TEXT("links"), Plan.CountRequestedLinks(), 1);
	TestEqual(TEXT("diagnostics"), Diagnostics.Num(), 0);
	return true;
}

#endif
