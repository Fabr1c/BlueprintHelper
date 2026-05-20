#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphMutationPlanPureDtoTest,
	"BlueprintHelper.GraphWrite.MutationPlan.PureDto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphMutationPlanPureDtoTest::RunTest(const FString&)
{
	FBlueprintGraphMutationPlan Plan;
	Plan.GraphName = TEXT("EventGraph");

	FBlueprintGraphMutationNodePlan NodePlan;
	NodePlan.NodeId = TEXT("print_001");
	NodePlan.NodeType = EParsedBlueprintNodeType::CallFunction;
	NodePlan.FunctionName = TEXT("PrintString");
	NodePlan.DefaultValues.Add(TEXT("InString"), TEXT("Hello"));
	Plan.Nodes.Add(NodePlan);

	FBlueprintGraphMutationLinkPlan LinkPlan;
	LinkPlan.FromId = TEXT("entry");
	LinkPlan.FromPin = TEXT("then");
	LinkPlan.ToId = TEXT("print_001");
	LinkPlan.ToPin = TEXT("execute");
	Plan.Links.Add(LinkPlan);

	TestTrue(TEXT("valid plan"), Plan.IsValid());
	TestEqual(TEXT("requested nodes"), Plan.CountRequestedNodes(), 1);
	TestEqual(TEXT("requested defaults"), Plan.CountRequestedDefaultValues(), 1);
	TestEqual(TEXT("requested links"), Plan.CountRequestedLinks(), 1);
	return true;
}

#endif
