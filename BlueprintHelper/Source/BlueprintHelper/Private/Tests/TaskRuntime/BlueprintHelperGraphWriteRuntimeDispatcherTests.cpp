#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteRuntimeDispatcher.h"

static TSharedRef<FJsonObject> MakeGraphWriteStepForDispatcherTest(const FString& Strategy)
{
	TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Test"));
	Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), Strategy);
	TArray<TSharedPtr<FJsonValue>> Ops;
	TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
	if (Strategy == TEXT("external_graph_edit"))
	{
		Op->SetStringField(TEXT("op"), TEXT("connect_external_pins"));
	}
	else
	{
		Op->SetStringField(TEXT("op"), TEXT("ensure_entry"));
		Op->SetStringField(TEXT("entry_type"), TEXT("custom_event"));
		Op->SetStringField(TEXT("name"), TEXT("DispatcherEntry"));
	}
	Ops.Add(MakeShared<FJsonValueObject>(Op));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);
	return Step;
}

static void AddOwnedConstraintsForDispatcherTest(const TSharedRef<FJsonObject>& Step)
{
	TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
	Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);
}

static void AddExternalConstraintsForDispatcherTest(const TSharedRef<FJsonObject>& Step)
{
	TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetStringField(TEXT("ownership_scope"), TEXT("external_user_authored"));
	Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
	TSharedRef<FJsonObject> Policy = MakeShared<FJsonObject>();
	Policy->SetStringField(TEXT("strategy"), TEXT("patch_external_links"));
	TArray<TSharedPtr<FJsonValue>> Mutations;
	Mutations.Add(MakeShared<FJsonValueString>(TEXT("link_connect")));
	Mutations.Add(MakeShared<FJsonValueString>(TEXT("link_disconnect")));
	Mutations.Add(MakeShared<FJsonValueString>(TEXT("link_replace")));
	Policy->SetArrayField(TEXT("allowed_mutations"), Mutations);
	Constraints->SetObjectField(TEXT("external_mutation_policy"), Policy);
	Step->SetObjectField(TEXT("constraints"), Constraints);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteRuntimeDispatcherRejectsUnsupportedStrategyTest,
	"BlueprintHelper.TaskRuntime.GraphWriteDispatcher.RejectsUnsupportedStrategy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteRuntimeDispatcherRejectsUnsupportedStrategyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteLoweringRequest Request;
	Request.TaskPlan = MakeShared<FJsonObject>();
	Request.StepObject = MakeGraphWriteStepForDispatcherTest(TEXT("invalid_strategy"));
	Request.bDryRun = true;

	FBlueprintHelperGraphWriteLoweringResult Result;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperGraphWriteRuntimeDispatcher::TryLower(Request, Result, Error);

	TestFalse(TEXT("invalid strategy rejected"), bLowered);
	TestEqual(TEXT("error code"), Error.Code, FString(TEXT("unsupported_graph_write_strategy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOwnedGraphMutationAdapterEnsureEntryTest,
	"BlueprintHelper.TaskRuntime.GraphWriteOwnedAdapter.EnsureEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOwnedGraphMutationAdapterEnsureEntryTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Step = MakeGraphWriteStepForDispatcherTest(TEXT("owned_graph_edit"));
	AddOwnedConstraintsForDispatcherTest(Step);

	FBlueprintHelperGraphWriteLoweringRequest Request;
	Request.TaskPlan = MakeShared<FJsonObject>();
	Request.StepObject = Step;
	Request.bDryRun = true;

	FBlueprintHelperGraphWriteLoweringResult Result;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperGraphWriteRuntimeDispatcher::TryLower(Request, Result, Error);

	TestTrue(TEXT("owned ensure_entry lowered"), bLowered);
	TestEqual(TEXT("adapter operation"), Result.LoweredStep.AdapterOperation, FString(TEXT("append_blueprint_graph")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExternalUserGraphMutationAdapterConnectPinsTest,
	"BlueprintHelper.TaskRuntime.GraphWriteExternalAdapter.ConnectPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperExternalUserGraphMutationAdapterConnectPinsTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Step = MakeGraphWriteStepForDispatcherTest(TEXT("external_graph_edit"));
	AddExternalConstraintsForDispatcherTest(Step);

	FBlueprintHelperGraphWriteLoweringRequest Request;
	Request.TaskPlan = MakeShared<FJsonObject>();
	Request.StepObject = Step;
	Request.bDryRun = true;

	FBlueprintHelperGraphWriteLoweringResult Result;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperGraphWriteRuntimeDispatcher::TryLower(Request, Result, Error);

	TestFalse(TEXT("incomplete connect anchors rejected before adapter operation is produced"), bLowered);
	TestEqual(TEXT("error points at source anchor"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].source_anchor")));
	return true;
}
