#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAcceptedPayloadModelIdentityTest,
	"BlueprintHelper.TaskRuntime.AcceptedPayloadModel.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAcceptedPayloadModelIdentityTest::RunTest(const FString&)
{
	FBlueprintHelperAcceptedPayloadModel Model;
	Model.TaskId = TEXT("task_001");
	Model.OperationId = TEXT("replace_blueprint_graph");
	Model.WriteFamily = TEXT("graphwrite");
	Model.RuntimeAdapterId = TEXT("graphwrite");
	Model.TaskSpecStrategy = TEXT("graphwrite_route_descriptor");
	Model.BridgeCommand = TEXT("execute_task_plan");
	Model.TargetAssetPath = TEXT("/Game/BH/BP_Target");
	Model.GraphName = TEXT("EventGraph");
	Model.Mode = TEXT("execute");

	const FString ScopeIdentity = FBlueprintHelperAcceptedPayloadModelUtils::MakeReviewScopeIdentity(Model);
	TestTrue(TEXT("scope uses asset"), ScopeIdentity.Contains(Model.TargetAssetPath));
	TestTrue(TEXT("scope uses graph"), ScopeIdentity.Contains(Model.GraphName));
	TestTrue(TEXT("debug trace is generated"), !FBlueprintHelperAcceptedPayloadModelUtils::MakeDebugTraceId(Model).IsEmpty());

	const TSharedRef<FJsonObject> Json = FBlueprintHelperAcceptedPayloadModelUtils::ToJson(Model);
	TestEqual(TEXT("json write family"), Json->GetStringField(TEXT("write_family")), FString(TEXT("graphwrite")));
	TestEqual(TEXT("json bridge command"), Json->GetStringField(TEXT("bridge_command")), FString(TEXT("execute_task_plan")));
	return true;
}

#endif
