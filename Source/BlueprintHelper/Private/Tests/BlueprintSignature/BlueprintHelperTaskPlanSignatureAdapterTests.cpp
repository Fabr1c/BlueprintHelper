#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"

namespace
{
	TSharedPtr<FJsonObject> MakeSignatureStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_function"));
		Op->SetStringField(TEXT("function_name"), TEXT("Interact"));
		Op->SetStringField(TEXT("interface_path"), TEXT("/Game/Interfaces/BPI_Interact"));
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("function_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	TSharedPtr<FJsonObject> MakeCustomEventSignatureStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_custom_event"));
		Op->SetStringField(TEXT("event_name"), TEXT("ToggleDoor"));
		Op->SetStringField(TEXT("graph_name"), TEXT("EG_DoorFeature"));
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("custom_event_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterEnsureFunctionTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.EnsureFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterEnsureFunctionTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeSignatureStep();

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("signature step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_signature"), LoweredStep.Capability, FString(TEXT("blueprint_signature")));
	TestEqual(TEXT("adapter operation is ensure_function"), LoweredStep.AdapterOperation, FString(TEXT("ensure_function")));
	TestTrue(TEXT("payload exists"), LoweredStep.Payload.IsValid());

	FString FunctionName;
	TestTrue(TEXT("payload carries function_name"), LoweredStep.Payload->TryGetStringField(TEXT("function_name"), FunctionName));
	TestEqual(TEXT("function name preserved"), FunctionName, FString(TEXT("Interact")));

	bool bDryRun = false;
	TestTrue(TEXT("payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("dry_run preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterEnsureCustomEventTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.EnsureCustomEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterEnsureCustomEventTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeCustomEventSignatureStep();

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("custom event signature step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_signature"), LoweredStep.Capability, FString(TEXT("blueprint_signature")));
	TestEqual(TEXT("adapter operation is ensure_custom_event"), LoweredStep.AdapterOperation, FString(TEXT("ensure_custom_event")));
	TestTrue(TEXT("payload exists"), LoweredStep.Payload.IsValid());

	FString EventName;
	TestTrue(TEXT("payload carries event_name"), LoweredStep.Payload->TryGetStringField(TEXT("event_name"), EventName));
	TestEqual(TEXT("event name preserved"), EventName, FString(TEXT("ToggleDoor")));

	FString GraphName;
	TestTrue(TEXT("payload carries graph_name"), LoweredStep.Payload->TryGetStringField(TEXT("graph_name"), GraphName));
	TestEqual(TEXT("graph name preserved"), GraphName, FString(TEXT("EG_DoorFeature")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeSignatureStep();
	Step->SetStringField(TEXT("operation"), TEXT("ensure_function"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("operation field is rejected"), bLowered);
	TestEqual(TEXT("error code"), Error.Code, FString(TEXT("unsupported_signature_operation_field")));
	TestEqual(TEXT("error stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	return true;
}

#endif
