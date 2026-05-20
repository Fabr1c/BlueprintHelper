#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteContextNullSafeTest,
	"BlueprintHelper.GraphWrite.Context.NullSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteContextNullSafeTest::RunTest(const FString&)
{
	FBlueprintGraphWriteContext Context;
	Context.Initialize(nullptr);

	TestFalse(TEXT("invalid graph context"), Context.IsValid());
	TestNull(TEXT("null node lookup"), Context.FindNode(TEXT("missing")));
	TestNull(TEXT("null pin lookup"), Context.FindPinByAlias(TEXT("missing"), TEXT("execute")));
	TestEqual(TEXT("generated node count"), Context.GetGeneratedNodes().Num(), 0);
	return true;
}

#endif
