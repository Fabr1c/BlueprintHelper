#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldActionReadbackSerializesCoreFactsTest,
	"BlueprintHelper.GraphWrite.FieldReadback.SerializesCoreFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldActionReadbackSerializesCoreFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFieldActionReadback Readback;
	Readback.CapabilityId = TEXT("field.member_get");
	Readback.NodeGuid = TEXT("11111111-2222-3333-4444-555555555555");
	Readback.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Readback.NodeTitle = TEXT("Get Health");
	Readback.ExpectedNodeFamily = TEXT("variable_get");
	Readback.Facts.Add(TEXT("field.member_reference.owner_class"), TEXT("/Script/Engine.Actor"));

	TMap<FString, FString> FlatFacts;
	Readback.AppendFlatFacts(FlatFacts);

	TestEqual(TEXT("capability fact"), FlatFacts.FindRef(TEXT("field.capability_id")), FString(TEXT("field.member_get")));
	TestEqual(TEXT("node class fact"), FlatFacts.FindRef(TEXT("field.node_class")), FString(TEXT("/Script/BlueprintGraph.K2Node_VariableGet")));
	TestEqual(TEXT("family fact"), FlatFacts.FindRef(TEXT("field.expected_node_family")), FString(TEXT("variable_get")));
	TestEqual(TEXT("member owner fact"), FlatFacts.FindRef(TEXT("field.member_reference.owner_class")), FString(TEXT("/Script/Engine.Actor")));
	return true;
}

#endif
