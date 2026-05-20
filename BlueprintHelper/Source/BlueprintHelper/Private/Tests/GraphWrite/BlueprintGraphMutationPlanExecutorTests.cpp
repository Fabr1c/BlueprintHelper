#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphMutationPlanExecutorNullContextTest,
	"BlueprintHelper.GraphWrite.MutationPlanExecutor.NullContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphMutationPlanExecutorNullContextTest::RunTest(const FString&)
{
	FBlueprintGraphWriteContext Context;
	FBlueprintGraphMutationPlan Plan;
	Plan.GraphName = TEXT("EventGraph");

	FBlueprintGenerateResult Result = FBlueprintGraphMutationPlanExecutor::Execute(Context, Plan);
	TestFalse(TEXT("execute fails"), Result.bSucceed);
	TestTrue(TEXT("message describes invalid context"), Result.Message.Contains(TEXT("GraphWrite context is invalid")));
	return true;
}

#endif
