#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_Knot.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteContextTracksGeneratedNodesTest,
	"BlueprintHelper.GraphWrite.Context.TracksGeneratedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteContextTracksGeneratedNodesTest::RunTest(const FString&)
{
	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Context_GeneratedNodes")));
	UK2Node_Knot* Node = NewObject<UK2Node_Knot>(Graph, FName(TEXT("GeneratedNode")));
	Graph->AddNode(Node, true, false);

	FBlueprintGraphWriteContext Context;
	Context.Initialize(Graph);
	Context.RegisterNode(TEXT("generated"), Node, true);

	TestEqual(TEXT("generated count"), Context.GetGeneratedNodes().Num(), 1);
	TestTrue(TEXT("generated node recorded"), Context.GetGeneratedNodes().Contains(Node));
	TestTrue(TEXT("node lookup returns registered node"), Context.FindNode(TEXT("generated")) == Node);

	Context.Initialize(nullptr);
	TestEqual(TEXT("generated nodes cleared"), Context.GetGeneratedNodes().Num(), 0);
	return true;
}

#endif
