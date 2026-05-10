#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"

class FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils
{
public:
	static TSharedPtr<FJsonObject> MakeSignatureStep()
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

		TSharedPtr<FJsonObject> InputPinType = MakeShared<FJsonObject>();
		InputPinType->SetStringField(TEXT("category"), TEXT("bool"));
		TSharedPtr<FJsonObject> Input = MakeShared<FJsonObject>();
		Input->SetStringField(TEXT("name"), TEXT("bPressed"));
		Input->SetObjectField(TEXT("pin_type"), InputPinType);
		TArray<TSharedPtr<FJsonValue>> Inputs;
		Inputs.Add(MakeShared<FJsonValueObject>(Input.ToSharedRef()));
		Op->SetArrayField(TEXT("inputs"), Inputs);

		TSharedPtr<FJsonObject> OutputPinType = MakeShared<FJsonObject>();
		OutputPinType->SetStringField(TEXT("category"), TEXT("bool"));
		TSharedPtr<FJsonObject> Output = MakeShared<FJsonObject>();
		Output->SetStringField(TEXT("name"), TEXT("bHandled"));
		Output->SetObjectField(TEXT("pin_type"), OutputPinType);
		TArray<TSharedPtr<FJsonValue>> Outputs;
		Outputs.Add(MakeShared<FJsonValueObject>(Output.ToSharedRef()));
		Op->SetArrayField(TEXT("outputs"), Outputs);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("function_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeCustomEventSignatureStep()
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
		Op->SetStringField(TEXT("interface_path"), TEXT("/Game/Interfaces/BPI_Door"));
		Op->SetStringField(TEXT("interface_entry_kind"), TEXT("event"));
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TSharedPtr<FJsonObject> InputPinType = MakeShared<FJsonObject>();
		InputPinType->SetStringField(TEXT("category"), TEXT("bool"));
		TSharedPtr<FJsonObject> Input = MakeShared<FJsonObject>();
		Input->SetStringField(TEXT("name"), TEXT("bPressed"));
		Input->SetObjectField(TEXT("pin_type"), InputPinType);
		TArray<TSharedPtr<FJsonValue>> Inputs;
		Inputs.Add(MakeShared<FJsonValueObject>(Input.ToSharedRef()));
		Op->SetArrayField(TEXT("inputs"), Inputs);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("custom_event_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeRemoveCustomEventSignatureStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature_remove"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("remove_signature"));
		Op->SetStringField(TEXT("signature_kind"), TEXT("custom_event"));
		Op->SetStringField(TEXT("signature_name"), TEXT("ToggleDoor"));
		Op->SetStringField(TEXT("graph_name"), TEXT("EG_DoorFeature"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("custom_event_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeRemoveSignatureStep(const FString& SignatureKind, const FString& SignatureName, const FString& Strategy)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature_remove_policy"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("remove_signature"));
		Op->SetStringField(TEXT("signature_kind"), SignatureKind);
		Op->SetStringField(TEXT("signature_name"), SignatureName);
		Op->SetStringField(TEXT("execute_policy"), TEXT("blocked_preflight"));
		Op->SetBoolField(TEXT("require_reference_context"), true);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), Strategy);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeEventDispatcherSignatureStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature_dispatcher"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_event_dispatcher"));
		Op->SetStringField(TEXT("dispatcher_name"), TEXT("OnDoorOpened"));
		Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));

		TSharedPtr<FJsonObject> InputPinType = MakeShared<FJsonObject>();
		InputPinType->SetStringField(TEXT("category"), TEXT("bool"));
		TSharedPtr<FJsonObject> Input = MakeShared<FJsonObject>();
		Input->SetStringField(TEXT("name"), TEXT("bIsOpen"));
		Input->SetObjectField(TEXT("pin_type"), InputPinType);
		TArray<TSharedPtr<FJsonValue>> Inputs;
		Inputs.Add(MakeShared<FJsonValueObject>(Input.ToSharedRef()));
		Op->SetArrayField(TEXT("inputs"), Inputs);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("event_dispatcher_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeOverrideEventSignatureStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_signature_override"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_signature"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_override_event"));
		Op->SetStringField(TEXT("event_name"), TEXT("ReceiveBeginPlay"));
		Op->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
		Op->SetStringField(TEXT("event_kind"), TEXT("native_event"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("override_event_signature"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterEnsureFunctionTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.EnsureFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterEnsureFunctionTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeSignatureStep();

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

	FString InterfacePath;
	TestTrue(TEXT("payload carries interface_path"), LoweredStep.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath));
	TestEqual(TEXT("interface path preserved"), InterfacePath, FString(TEXT("/Game/Interfaces/BPI_Interact")));

	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	TestTrue(TEXT("payload carries inputs"), LoweredStep.Payload->TryGetArrayField(TEXT("inputs"), Inputs));
	TestTrue(TEXT("payload inputs not empty"), Inputs && Inputs->Num() == 1);

	const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
	TestTrue(TEXT("payload carries outputs"), LoweredStep.Payload->TryGetArrayField(TEXT("outputs"), Outputs));
	TestTrue(TEXT("payload outputs not empty"), Outputs && Outputs->Num() == 1);

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
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeCustomEventSignatureStep();

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

	FString InterfacePath;
	TestTrue(TEXT("payload carries interface_path"), LoweredStep.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath));
	TestEqual(TEXT("interface path preserved"), InterfacePath, FString(TEXT("/Game/Interfaces/BPI_Door")));

	FString InterfaceEntryKind;
	TestTrue(TEXT("payload carries interface_entry_kind"), LoweredStep.Payload->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind));
	TestEqual(TEXT("interface entry kind preserved"), InterfaceEntryKind, FString(TEXT("event")));

	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	TestTrue(TEXT("payload carries inputs"), LoweredStep.Payload->TryGetArrayField(TEXT("inputs"), Inputs));
	TestTrue(TEXT("payload inputs not empty"), Inputs && Inputs->Num() == 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRemoveSignatureTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RemoveSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRemoveSignatureTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeRemoveCustomEventSignatureStep();

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("remove signature step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_signature"), LoweredStep.Capability, FString(TEXT("blueprint_signature")));
	TestEqual(TEXT("adapter operation is remove_signature"), LoweredStep.AdapterOperation, FString(TEXT("remove_signature")));
	TestTrue(TEXT("payload exists"), LoweredStep.Payload.IsValid());

	FString SignatureKind;
	TestTrue(TEXT("payload carries signature_kind"), LoweredStep.Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind));
	TestEqual(TEXT("signature kind preserved"), SignatureKind, FString(TEXT("custom_event")));

	FString SignatureName;
	TestTrue(TEXT("payload carries signature_name"), LoweredStep.Payload->TryGetStringField(TEXT("signature_name"), SignatureName));
	TestEqual(TEXT("signature name preserved"), SignatureName, FString(TEXT("ToggleDoor")));

	FString GraphName;
	TestTrue(TEXT("payload carries graph_name"), LoweredStep.Payload->TryGetStringField(TEXT("graph_name"), GraphName));
	TestEqual(TEXT("graph name preserved"), GraphName, FString(TEXT("EG_DoorFeature")));

	bool bDryRun = false;
	TestTrue(TEXT("payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("dry_run preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRemoveDispatcherSignatureTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RemoveDispatcherSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRemoveDispatcherSignatureTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeRemoveSignatureStep(
		TEXT("event_dispatcher"),
		TEXT("OnDoorOpened"),
		TEXT("event_dispatcher_signature"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("event dispatcher remove signature step lowers successfully"), bLowered);
	TestEqual(TEXT("adapter operation is remove_signature"), LoweredStep.AdapterOperation, FString(TEXT("remove_signature")));

	FString SignatureKind;
	TestTrue(TEXT("payload carries signature_kind"), LoweredStep.Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind));
	TestEqual(TEXT("signature kind preserved"), SignatureKind, FString(TEXT("event_dispatcher")));

	bool bRequireReferenceContext = false;
	TestTrue(TEXT("payload carries require_reference_context"), LoweredStep.Payload->TryGetBoolField(TEXT("require_reference_context"), bRequireReferenceContext));
	TestTrue(TEXT("remove requires reference context"), bRequireReferenceContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRejectsRemoveWithoutReferenceContextTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RejectsRemoveWithoutReferenceContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRejectsRemoveWithoutReferenceContextTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeRemoveSignatureStep(
		TEXT("event_dispatcher"),
		TEXT("OnDoorOpened"),
		TEXT("event_dispatcher_signature"));

	const TSharedPtr<FJsonObject>* WriteObject = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	TestTrue(TEXT("step carries write object"), Step->TryGetObjectField(TEXT("write"), WriteObject) && WriteObject && WriteObject->IsValid());
	TestTrue(TEXT("write carries ops"), WriteObject && (*WriteObject)->TryGetArrayField(TEXT("ops"), Ops) && Ops && Ops->Num() == 1);
	if (!WriteObject || !WriteObject->IsValid() || !Ops || Ops->Num() != 1)
	{
		return false;
	}

	TSharedPtr<FJsonObject> OpObject = (*Ops)[0]->AsObject();
	TestTrue(TEXT("op object exists"), OpObject.IsValid());
	if (!OpObject.IsValid())
	{
		return false;
	}
	OpObject->SetBoolField(TEXT("require_reference_context"), false);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("remove without reference context is rejected"), bLowered);
	TestEqual(TEXT("error code"), Error.Code, FString(TEXT("invalid_signature_remove_policy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRemoveOverrideSignatureTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RemoveOverrideSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRemoveOverrideSignatureTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeRemoveSignatureStep(
		TEXT("native_event"),
		TEXT("ReceiveBeginPlay"),
		TEXT("override_event_signature"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("override/native remove signature step lowers successfully"), bLowered);
	TestEqual(TEXT("adapter operation is remove_signature"), LoweredStep.AdapterOperation, FString(TEXT("remove_signature")));

	FString SignatureKind;
	TestTrue(TEXT("payload carries signature_kind"), LoweredStep.Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind));
	TestEqual(TEXT("signature kind preserved"), SignatureKind, FString(TEXT("native_event")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterEnsureEventDispatcherTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.EnsureEventDispatcher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterEnsureEventDispatcherTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeEventDispatcherSignatureStep();

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("event dispatcher signature step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_signature"), LoweredStep.Capability, FString(TEXT("blueprint_signature")));
	TestEqual(TEXT("adapter operation is ensure_event_dispatcher"), LoweredStep.AdapterOperation, FString(TEXT("ensure_event_dispatcher")));
	TestTrue(TEXT("payload exists"), LoweredStep.Payload.IsValid());

	FString DispatcherName;
	TestTrue(TEXT("payload carries dispatcher_name"), LoweredStep.Payload->TryGetStringField(TEXT("dispatcher_name"), DispatcherName));
	TestEqual(TEXT("dispatcher name preserved"), DispatcherName, FString(TEXT("OnDoorOpened")));

	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	TestTrue(TEXT("payload carries inputs"), LoweredStep.Payload->TryGetArrayField(TEXT("inputs"), Inputs));
	TestTrue(TEXT("payload inputs not empty"), Inputs && Inputs->Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterEnsureOverrideEventBlockedPathTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.EnsureOverrideEventBlockedPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterEnsureOverrideEventBlockedPathTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeOverrideEventSignatureStep();

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("override event signature step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_signature"), LoweredStep.Capability, FString(TEXT("blueprint_signature")));
	TestEqual(TEXT("adapter operation is ensure_override_event"), LoweredStep.AdapterOperation, FString(TEXT("ensure_override_event")));
	TestTrue(TEXT("payload exists"), LoweredStep.Payload.IsValid());

	FString EventName;
	TestTrue(TEXT("payload carries event_name"), LoweredStep.Payload->TryGetStringField(TEXT("event_name"), EventName));
	TestEqual(TEXT("event name preserved"), EventName, FString(TEXT("ReceiveBeginPlay")));

	FString EventKind;
	TestTrue(TEXT("payload carries event_kind"), LoweredStep.Payload->TryGetStringField(TEXT("event_kind"), EventKind));
	TestEqual(TEXT("event kind preserved"), EventKind, FString(TEXT("native_event")));

	FString GraphName;
	TestTrue(TEXT("payload carries graph_name"), LoweredStep.Payload->TryGetStringField(TEXT("graph_name"), GraphName));
	TestEqual(TEXT("graph name preserved"), GraphName, FString(TEXT("EventGraph")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanSignatureAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.SignatureAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanSignatureAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanSignatureAdapterTestsLocalUtils::MakeSignatureStep();
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
