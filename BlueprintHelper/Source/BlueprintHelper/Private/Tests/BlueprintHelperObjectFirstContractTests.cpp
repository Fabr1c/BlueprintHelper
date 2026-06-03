#if WITH_DEV_AUTOMATION_TESTS

#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Shared/BlueprintHelperDependencyAnalysisTypes.h"
#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperObjectFirstContractTestsLocalUtils
{
public:
	static TSharedPtr<FJsonObject> MakeLiteralValue(const FString& ValueType, const TSharedPtr<FJsonValue>& Value)
	{
		TSharedPtr<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), ValueType);
		Literal->SetField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedPtr<FJsonObject> MakeGraphWriteEnsureEntryStep(bool bIncludeLegacyOperation = false)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_001"));
		if (bIncludeLegacyOperation)
		{
			Step->SetStringField(TEXT("operation"), TEXT("append_blueprint_graph"));
		}
		Step->SetStringField(TEXT("capability"), TEXT("graph_write"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
		Target->SetStringField(TEXT("graph"), TEXT("BH_StoneGateActivation"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> CallArgsOne = MakeShared<FJsonObject>();
		CallArgsOne->SetObjectField(
			TEXT("bNewActorEnableCollision"),
			MakeLiteralValue(TEXT("bool"), MakeShared<FJsonValueBoolean>(true)));

		TSharedPtr<FJsonObject> CallStatementOne = MakeShared<FJsonObject>();
		CallStatementOne->SetStringField(TEXT("kind"), TEXT("call_function"));
		CallStatementOne->SetStringField(TEXT("name"), TEXT("SetActorEnableCollision"));
		CallStatementOne->SetObjectField(TEXT("args"), CallArgsOne);

		TSharedPtr<FJsonObject> SetStatement = MakeShared<FJsonObject>();
		SetStatement->SetStringField(TEXT("kind"), TEXT("set_member_variable"));
		SetStatement->SetStringField(TEXT("name"), TEXT("bGateUnlocked"));
		SetStatement->SetObjectField(
			TEXT("value"),
			MakeLiteralValue(TEXT("bool"), MakeShared<FJsonValueBoolean>(false)));

		TSharedPtr<FJsonObject> CallArgsTwo = MakeShared<FJsonObject>();
		CallArgsTwo->SetObjectField(
			TEXT("InString"),
			MakeLiteralValue(TEXT("string"), MakeShared<FJsonValueString>(TEXT("Stone gate initialized"))));
		CallArgsTwo->SetObjectField(
			TEXT("Duration"),
			MakeLiteralValue(TEXT("float"), MakeShared<FJsonValueNumber>(2.0)));

		TSharedPtr<FJsonObject> CallStatementTwo = MakeShared<FJsonObject>();
		CallStatementTwo->SetStringField(TEXT("kind"), TEXT("call_function"));
		CallStatementTwo->SetStringField(TEXT("name"), TEXT("PrintString"));
		CallStatementTwo->SetObjectField(TEXT("args"), CallArgsTwo);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(CallStatementOne.ToSharedRef()));
		Statements.Add(MakeShared<FJsonValueObject>(SetStatement.ToSharedRef()));
		Statements.Add(MakeShared<FJsonValueObject>(CallStatementTwo.ToSharedRef()));

		TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));
		Body->SetArrayField(TEXT("statements"), Statements);

		TSharedPtr<FJsonObject> EnsureEntry = MakeShared<FJsonObject>();
		EnsureEntry->SetStringField(TEXT("op"), TEXT("ensure_entry"));
		EnsureEntry->SetStringField(TEXT("entry_type"), TEXT("custom_event"));
		EnsureEntry->SetStringField(TEXT("name"), TEXT("InitializeStoneGate"));
		EnsureEntry->SetObjectField(TEXT("body"), Body);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(EnsureEntry.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("owned_graph_edit"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
		Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
		Step->SetObjectField(TEXT("constraints"), Constraints);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeGraphWriteTaskPlan(const TSharedPtr<FJsonObject>& Step)
	{
		TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("StoneGateActivation"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_blueprint_graph"));
		TaskPlan->SetStringField(TEXT("context_id"), TEXT("ctx_stone_gate_activation"));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Blueprints/BP_StoneGate")));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

		TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), true);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(Step.ToSharedRef()));
		TaskPlan->SetArrayField(TEXT("steps"), Steps);
		return TaskPlan;
	}

	static TSharedPtr<FJsonObject> MakeTaskPlanWithSteps(
		const TArray<TSharedPtr<FJsonObject>>& StepsToAdd,
		const FString& TaskName = TEXT("StoneGateActivation"),
		const FString& TaskType = TEXT("edit_blueprint_graph"))
	{
		TSharedPtr<FJsonObject> TaskPlan = MakeGraphWriteTaskPlan(StepsToAdd.Num() > 0 ? StepsToAdd[0] : MakeGraphWriteEnsureEntryStep());
		TaskPlan->SetStringField(TEXT("task_name"), TaskName);
		TaskPlan->SetStringField(TEXT("task_type"), TaskType);

		TArray<TSharedPtr<FJsonValue>> Steps;
		for (const TSharedPtr<FJsonObject>& Step : StepsToAdd)
		{
			Steps.Add(MakeShared<FJsonValueObject>(Step.ToSharedRef()));
		}
		TaskPlan->SetArrayField(TEXT("steps"), Steps);
		return TaskPlan;
	}

	static TSharedPtr<FJsonObject> MakeGraphWriteStepWithSingleOp(const TSharedPtr<FJsonObject>& Op)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_001"));
		Step->SetStringField(TEXT("capability"), TEXT("graph_write"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
		Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
		Step->SetObjectField(TEXT("target"), Target);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("owned_graph_edit"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_modify_user_nodes"), false);
		Constraints->SetStringField(TEXT("ownership_scope"), TEXT("blueprinthelper_owned"));
		Step->SetObjectField(TEXT("constraints"), Constraints);
		return Step;
	}

	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return JsonText;
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractExportIncludeJsonTextTest,
	"BlueprintHelper.ObjectFirst.Contract.ExportRejectsIncludeJsonText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractExportIncludeJsonTextTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	// Requests without include_json_text are accepted.
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("target_blueprint"), TEXT("/Game/BP/Test"));
		Payload->SetStringField(TEXT("target_graph"), TEXT("EventGraph"));
		Payload->SetStringField(TEXT("scope"), TEXT("graph"));

		TestTrue(TEXT("export_to_json accepts payload without include_json_text"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text is retired and no longer accepted.
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetBoolField(TEXT("include_json_text"), true);

		TestFalse(TEXT("export_to_json rejects include_json_text"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text is rejected even when false.
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetBoolField(TEXT("include_json_text"), false);

		TestFalse(TEXT("export_to_json rejects include_json_text:false"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text 涓?string 绫诲瀷搴旇鎷掔粷
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("include_json_text"), TEXT("not_a_bool"));

		TestFalse(TEXT("export_to_json 鎷掔粷 string 绫诲瀷鐨?include_json_text"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskPlanPayloadTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskPlanPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskPlanPayloadTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), MakeShared<FJsonObject>());

		TestTrue(TEXT("preview_task_plan 鎺ュ彈 task_plan object"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("preview_task_plan"), Payload, Error));
		TestTrue(TEXT("execute_task_plan 鎺ュ彈 task_plan object"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("execute_task_plan"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

		TestFalse(TEXT("preview_task_plan 鎷掔粷缂哄皯 task_plan"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("preview_task_plan"), Payload, Error));
		TestEqual(TEXT("preview_task_plan 缂哄け瀛楁瀹氫綅鍒?payload.task_plan"),
			Error.Field, FString(TEXT("payload.task_plan")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("task_run_id"), TEXT("task_001"));

		TestTrue(TEXT("get_task_run_journal 鎺ュ彈 task_run_id"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("get_task_run_journal"), Payload, Error));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskPlanWriteBoundaryTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskPlanWriteBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskPlanWriteBoundaryTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("preview_task_plan is not a write command"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("preview_task_plan")));
	TestTrue(TEXT("execute_task_plan 鏄啓鍛戒护"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("execute_task_plan")));
	TestFalse(TEXT("get_task_run_journal is not a write command"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("get_task_run_journal")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeExecutionPolicyValidationTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeExecutionPolicyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeExecutionPolicyValidationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperValidationSummary BaseValidation;
	BaseValidation.bShouldCompile = true;
	BaseValidation.bShouldSave = true;

	TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
	ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
	ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

	TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

	const FBlueprintHelperValidationSummary RuntimeValidation =
		FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(TaskPlan, BaseValidation);

	TestFalse(TEXT("TaskRuntime 浣跨敤 execution_policy.should_compile"),
		RuntimeValidation.bShouldCompile);
	TestFalse(TEXT("TaskRuntime 浣跨敤 execution_policy.should_save"),
		RuntimeValidation.bShouldSave);

	TSharedPtr<FJsonObject> TaskPlanWithoutPolicy = MakeShared<FJsonObject>();
	const FBlueprintHelperValidationSummary FallbackValidation =
		FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(TaskPlanWithoutPolicy, BaseValidation);

	TestTrue(TEXT("缂哄皯 execution_policy 鏃朵繚鐣欏熀纭€ should_compile"),
		FallbackValidation.bShouldCompile);
	TestTrue(TEXT("缂哄皯 execution_policy 鏃朵繚鐣欏熀纭€ should_save"),
		FallbackValidation.bShouldSave);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrLoweringTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep();
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("graph_write owned_graph_edit ensure_entry lowers successfully"), bLowered);
	TestEqual(TEXT("graph_write step reports capability"), LoweredStep.Capability, FString(TEXT("graph_write")));
	TestEqual(TEXT("graph_write runtime step operation reports graph_write"), LoweredStep.RuntimeOperation, FString(TEXT("graph_write")));
	TestEqual(TEXT("graph_write lowers to append adapter"), LoweredStep.AdapterOperation, FString(TEXT("append_blueprint_graph")));
	TestNotNull(TEXT("lowered append payload exists"), LoweredStep.Payload.Get());

	const TSharedPtr<FJsonObject> Payload = LoweredStep.Payload;
	FString FeatureName;
	TestTrue(TEXT("lowered payload carries feature_name"), Payload->TryGetStringField(TEXT("feature_name"), FeatureName));
	TestEqual(TEXT("feature_name comes from task_name"), FeatureName, FString(TEXT("StoneGateActivation")));

	bool bDryRun = false;
	TestTrue(TEXT("lowered payload injects dry_run"), Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("preview lowers with dry_run=true"), bDryRun);

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	TestTrue(TEXT("lowered payload contains target"), Payload->TryGetObjectField(TEXT("target"), TargetObject));
	TestNotNull(TEXT("lowered target object exists"), TargetObject != nullptr ? TargetObject->Get() : nullptr);

	FString AssetPath;
	FString GraphName;
	TestTrue(TEXT("target.asset_path preserved"), (*TargetObject)->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("target.graph preserved"), (*TargetObject)->TryGetStringField(TEXT("graph"), GraphName));
	TestEqual(TEXT("target.asset_path matches task plan"), AssetPath, FString(TEXT("/Game/Blueprints/BP_StoneGate")));
	TestEqual(TEXT("target.graph matches task plan"), GraphName, FString(TEXT("BH_StoneGateActivation")));

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	TestTrue(TEXT("lowered payload contains nodes"), Payload->TryGetArrayField(TEXT("nodes"), Nodes));
	TestTrue(TEXT("lowered payload contains links"), Payload->TryGetArrayField(TEXT("links"), Links));
	TestEqual(TEXT("ensure_entry lowers entry plus three statements"), Nodes->Num(), 4);
	TestEqual(TEXT("ensure_entry lowers one exec link per statement"), Links->Num(), 3);

	const TSharedPtr<FJsonObject> EntryNode = (*Nodes)[0]->AsObject();
	const TSharedPtr<FJsonObject> FirstCallNode = (*Nodes)[1]->AsObject();
	const TSharedPtr<FJsonObject> SetNode = (*Nodes)[2]->AsObject();
	const TSharedPtr<FJsonObject> SecondCallNode = (*Nodes)[3]->AsObject();

	FString EntryKind;
	FString EntryName;
	TestTrue(TEXT("entry node kind recorded"), EntryNode->TryGetStringField(TEXT("kind"), EntryKind));
	TestTrue(TEXT("entry node name recorded"), EntryNode->TryGetStringField(TEXT("name"), EntryName));
	TestEqual(TEXT("entry node lowers to custom_event"), EntryKind, FString(TEXT("custom_event")));
	TestEqual(TEXT("entry name preserved"), EntryName, FString(TEXT("InitializeStoneGate")));

	FString CallKind;
	FString CallFunction;
	TestTrue(TEXT("call node kind recorded"), FirstCallNode->TryGetStringField(TEXT("kind"), CallKind));
	TestTrue(TEXT("call node function recorded"), FirstCallNode->TryGetStringField(TEXT("function"), CallFunction));
	TestEqual(TEXT("call_function lowers to call node"), CallKind, FString(TEXT("call")));
	TestEqual(TEXT("call_function name preserved"), CallFunction, FString(TEXT("SetActorEnableCollision")));

	const TSharedPtr<FJsonObject>* CallInputs = nullptr;
	TestTrue(TEXT("call node contains inputs"), FirstCallNode->TryGetObjectField(TEXT("inputs"), CallInputs));
	bool bEnableCollision = false;
	TestTrue(TEXT("literal bool arg lowered into inputs"), (*CallInputs)->TryGetBoolField(TEXT("bNewActorEnableCollision"), bEnableCollision));
	TestTrue(TEXT("bool arg value preserved"), bEnableCollision);

	FString SetKind;
	FString SetVariable;
	FString SetValue;
	TestTrue(TEXT("set node kind recorded"), SetNode->TryGetStringField(TEXT("kind"), SetKind));
	TestTrue(TEXT("set node variable recorded"), SetNode->TryGetStringField(TEXT("var"), SetVariable));
	TestTrue(TEXT("set node value recorded"), SetNode->TryGetStringField(TEXT("value"), SetValue));
	TestEqual(TEXT("set_member_variable lowers to set node"), SetKind, FString(TEXT("set")));
	TestEqual(TEXT("set_member_variable variable preserved"), SetVariable, FString(TEXT("bGateUnlocked")));
	TestEqual(TEXT("set_member_variable bool literal lowers to string"), SetValue, FString(TEXT("false")));

	const TSharedPtr<FJsonObject>* SecondCallInputs = nullptr;
	TestTrue(TEXT("second call contains inputs"), SecondCallNode->TryGetObjectField(TEXT("inputs"), SecondCallInputs));
	FString InString;
	double Duration = 0.0;
	TestTrue(TEXT("string literal arg lowered into inputs"), (*SecondCallInputs)->TryGetStringField(TEXT("InString"), InString));
	TestTrue(TEXT("numeric literal arg lowered into inputs"), (*SecondCallInputs)->TryGetNumberField(TEXT("Duration"), Duration));
	TestEqual(TEXT("string literal preserved"), InString, FString(TEXT("Stone gate initialized")));
	TestEqual(TEXT("float literal preserved"), Duration, 2.0);

	FBlueprintHelperToolResultBase ChildResult = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("append_blueprint_graph"),
		TEXT("trace_graphwrite_ir"));
	ChildResult.Data = MakeShared<FJsonObject>();

	const TSharedRef<FJsonObject> RuntimeData = FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForStep(
		TaskPlan,
		TEXT(""),
		LoweredStep,
		ChildResult,
		true);
	const TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForStep(
		TEXT("task_graphwrite_ir"),
		TaskPlan,
		LoweredStep,
		ChildResult);

	const TArray<TSharedPtr<FJsonValue>>* RuntimeSteps = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JournalSteps = nullptr;
	TestTrue(TEXT("runtime data contains steps"), RuntimeData->TryGetArrayField(TEXT("steps"), RuntimeSteps));
	TestTrue(TEXT("journal contains steps"), Journal->TryGetArrayField(TEXT("steps"), JournalSteps));

	const TSharedPtr<FJsonObject> RuntimeStep = (*RuntimeSteps)[0]->AsObject();
	const TSharedPtr<FJsonObject> JournalStep = (*JournalSteps)[0]->AsObject();
	FString RuntimeCapability;
	FString RuntimeOperation;
	FString RuntimeAdapterOperation;
	FString JournalCapability;
	FString JournalOperation;
	FString JournalAdapterOperation;
	TestTrue(TEXT("runtime step includes capability"), RuntimeStep->TryGetStringField(TEXT("capability"), RuntimeCapability));
	TestTrue(TEXT("runtime step includes operation"), RuntimeStep->TryGetStringField(TEXT("operation"), RuntimeOperation));
	TestTrue(TEXT("runtime step includes adapter_operation"), RuntimeStep->TryGetStringField(TEXT("adapter_operation"), RuntimeAdapterOperation));
	TestTrue(TEXT("journal step includes capability"), JournalStep->TryGetStringField(TEXT("capability"), JournalCapability));
	TestTrue(TEXT("journal step includes operation"), JournalStep->TryGetStringField(TEXT("operation"), JournalOperation));
	TestTrue(TEXT("journal step includes adapter_operation"), JournalStep->TryGetStringField(TEXT("adapter_operation"), JournalAdapterOperation));
	TestEqual(TEXT("runtime step reports graph_write capability"), RuntimeCapability, FString(TEXT("graph_write")));
	TestEqual(TEXT("runtime step reports graph_write operation"), RuntimeOperation, FString(TEXT("graph_write")));
	TestEqual(TEXT("runtime step keeps adapter operation"), RuntimeAdapterOperation, FString(TEXT("append_blueprint_graph")));
	TestEqual(TEXT("journal step reports graph_write capability"), JournalCapability, FString(TEXT("graph_write")));
	TestEqual(TEXT("journal step reports graph_write operation"), JournalOperation, FString(TEXT("graph_write")));
	TestEqual(TEXT("journal step keeps adapter operation"), JournalAdapterOperation, FString(TEXT("append_blueprint_graph")));

	const TSharedPtr<FJsonObject>* RuntimeChildResult = nullptr;
	const TSharedPtr<FJsonObject>* JournalChildResult = nullptr;
	TestTrue(TEXT("runtime step exposes child adapter result"), RuntimeStep->TryGetObjectField(TEXT("result"), RuntimeChildResult));
	TestTrue(TEXT("journal step exposes child adapter result"), JournalStep->TryGetObjectField(TEXT("result"), JournalChildResult));

	FString RuntimeChildOperation;
	FString JournalChildOperation;
	TestTrue(TEXT("runtime child adapter result keeps append op"), (*RuntimeChildResult)->TryGetStringField(TEXT("operation"), RuntimeChildOperation));
	TestTrue(TEXT("journal child adapter result keeps append op"), (*JournalChildResult)->TryGetStringField(TEXT("operation"), JournalChildOperation));
	TestEqual(TEXT("runtime child adapter operation is append_blueprint_graph"), RuntimeChildOperation, FString(TEXT("append_blueprint_graph")));
	TestEqual(TEXT("journal child adapter operation is append_blueprint_graph"), JournalChildOperation, FString(TEXT("append_blueprint_graph")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteAppendDependsOnReusesEntryTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteAppendDependsOnReusesEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteAppendDependsOnReusesEntryTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep();
	Step->SetStringField(TEXT("step_id"), TEXT("step_002"));

	TArray<TSharedPtr<FJsonValue>> DependsOn;
	DependsOn.Add(MakeShared<FJsonValueString>(TEXT("step_001")));
	Step->SetArrayField(TEXT("depends_on"), DependsOn);

	TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("dependent graph_write append step lowers successfully"), bLowered);
	TestEqual(TEXT("dependent append still uses append adapter"), LoweredStep.AdapterOperation, FString(TEXT("append_blueprint_graph")));

	bool bReuseExistingEntries = false;
	TestTrue(TEXT("dependent append payload declares existing entry reuse"),
		LoweredStep.Payload.IsValid() &&
		LoweredStep.Payload->TryGetBoolField(TEXT("reuse_existing_entries"), bReuseExistingEntries));
	TestTrue(TEXT("dependent append payload reuses signature-created entries"), bReuseExistingEntries);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeAggregatesMultipleStepsTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeAggregatesMultipleSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeAggregatesMultipleStepsTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep());

	FBlueprintHelperTaskRuntimeLoweredStep FirstStep;
	FirstStep.StepId = TEXT("step_001");
	FirstStep.Capability = TEXT("graph_write");
	FirstStep.RuntimeOperation = TEXT("graph_write");
	FirstStep.AdapterOperation = TEXT("append_blueprint_graph");
	FBlueprintHelperToolResultBase FirstResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("append_blueprint_graph"),
		TEXT("trace_step_001"));

	FBlueprintHelperTaskRuntimeLoweredStep SecondStep;
	SecondStep.StepId = TEXT("step_002");
	SecondStep.Capability = TEXT("blueprint_variable");
	SecondStep.RuntimeOperation = TEXT("blueprint_variable");
	SecondStep.AdapterOperation = TEXT("add_blueprint_member_variables");
	FBlueprintHelperToolResultBase SecondResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("add_blueprint_member_variables"),
		TEXT("trace_step_002"));

	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	StepRecords.Add({FirstStep, FirstResult});
	StepRecords.Add({SecondStep, SecondResult});

	const TSharedRef<FJsonObject> RuntimeData = FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForSteps(
		TaskPlan,
		TEXT("task_multi_step"),
		StepRecords,
		{},
		false);
	const TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
		TEXT("task_multi_step"),
		TaskPlan,
		StepRecords,
		{});

	const TArray<TSharedPtr<FJsonValue>>* RuntimeSteps = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JournalSteps = nullptr;
	TestTrue(TEXT("runtime data exposes aggregated steps"), RuntimeData->TryGetArrayField(TEXT("steps"), RuntimeSteps));
	TestTrue(TEXT("journal exposes aggregated steps"), Journal->TryGetArrayField(TEXT("steps"), JournalSteps));
	TestEqual(TEXT("runtime data keeps both step results"), RuntimeSteps->Num(), 2);
	TestEqual(TEXT("journal keeps both step results"), JournalSteps->Num(), 2);

	FString RuntimeTaskRunId;
	FString JournalTaskRunId;
	TestTrue(TEXT("runtime data exposes task_run_id"), RuntimeData->TryGetStringField(TEXT("task_run_id"), RuntimeTaskRunId));
	TestTrue(TEXT("journal exposes task_run_id"), Journal->TryGetStringField(TEXT("task_run_id"), JournalTaskRunId));
	TestEqual(TEXT("runtime task_run_id matches"), RuntimeTaskRunId, FString(TEXT("task_multi_step")));
	TestEqual(TEXT("journal task_run_id matches"), JournalTaskRunId, FString(TEXT("task_multi_step")));

	FString JournalStatus;
	TestTrue(TEXT("journal status exists"), Journal->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("all successful steps complete journal"), JournalStatus, FString(TEXT("completed")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimePartialFailureJournalTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimePartialFailureJournal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimePartialFailureJournalTest::RunTest(const FString& Parameters)
{
	auto MakeCapabilityStep = [](const FString& StepId, const FString& Capability, const TArray<FString>& DependsOn)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), Capability);

		if (DependsOn.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> DependsOnValues;
			for (const FString& DependsOnStepId : DependsOn)
			{
				DependsOnValues.Add(MakeShared<FJsonValueString>(DependsOnStepId));
			}
			Step->SetArrayField(TEXT("depends_on"), DependsOnValues);
		}

		return Step;
	};

	const TArray<FString> NoDependencies;
	TArray<FString> FailedStepDependency;
	FailedStepDependency.Add(TEXT("step_append_graph"));
	const TSharedPtr<FJsonObject> FailedPlanStep = MakeCapabilityStep(
		TEXT("step_append_graph"),
		TEXT("graph_write"),
		NoDependencies);
	const TSharedPtr<FJsonObject> BlockedPlanStep = MakeCapabilityStep(
		TEXT("step_configure_variable"),
		TEXT("blueprint_variable"),
		FailedStepDependency);
	const TSharedPtr<FJsonObject> IndependentPlanStep = MakeCapabilityStep(
		TEXT("step_create_asset"),
		TEXT("asset_factory"),
		NoDependencies);
	TArray<TSharedPtr<FJsonObject>> PlannedSteps;
	PlannedSteps.Add(FailedPlanStep);
	PlannedSteps.Add(BlockedPlanStep);
	PlannedSteps.Add(IndependentPlanStep);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeTaskPlanWithSteps(
		PlannedSteps,
		TEXT("DoorFeature"),
		TEXT("create_blueprint_feature"));

	FBlueprintHelperTaskRuntimeLoweredStep FailedStep;
	FailedStep.StepId = TEXT("step_append_graph");
	FailedStep.Capability = TEXT("graph_write");
	FailedStep.RuntimeOperation = TEXT("graph_write");
	FailedStep.AdapterOperation = TEXT("append_blueprint_graph");

	FBlueprintHelperToolError FailedError;
	FailedError.Code = TEXT("append_failed");
	FailedError.Stage = EBlueprintHelperToolStage::Execute;
	FailedError.Message = TEXT("append step failed");
	FailedError.bRetryable = false;
	FailedError.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
	FBlueprintHelperToolResultBase FailedResult = FBlueprintHelperToolResultBuilder::Failure(
		TEXT("append_blueprint_graph"),
		TEXT("trace_failed_step"),
		FailedError);

	FBlueprintHelperTaskRuntimeLoweredStep IndependentStep;
	IndependentStep.StepId = TEXT("step_create_asset");
	IndependentStep.Capability = TEXT("asset_factory");
	IndependentStep.RuntimeOperation = TEXT("asset_factory");
	IndependentStep.AdapterOperation = TEXT("create_asset");
	FBlueprintHelperToolResultBase IndependentResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("create_asset"),
		TEXT("trace_independent_step"));

	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	StepRecords.Add({FailedStep, FailedResult});
	StepRecords.Add({IndependentStep, IndependentResult});

	const TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
		TEXT("task_partial_failure"),
		TaskPlan,
		StepRecords,
		{});

	FString JournalStatus;
	TestTrue(TEXT("journal status exists"), Journal->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("failed step produces partial_failure journal"), JournalStatus, FString(TEXT("partial_failure")));

	const TSharedPtr<FJsonObject>* Recovery = nullptr;
	TestTrue(TEXT("partial failure journal includes recovery"), Journal->TryGetObjectField(TEXT("recovery"), Recovery));
	FString RecommendedAction;
	bool bSafeToRetry = true;
	bool bRollbackAvailable = true;
	const TArray<TSharedPtr<FJsonValue>>* RecoveryNotes = nullptr;
	TestTrue(TEXT("recovery includes recommended_action"), (*Recovery)->TryGetStringField(TEXT("recommended_action"), RecommendedAction));
	TestTrue(TEXT("recovery includes safe_to_retry"), (*Recovery)->TryGetBoolField(TEXT("safe_to_retry"), bSafeToRetry));
	TestTrue(TEXT("recovery includes rollback_available"), (*Recovery)->TryGetBoolField(TEXT("rollback_available"), bRollbackAvailable));
	TestTrue(TEXT("recovery includes notes array"), (*Recovery)->TryGetArrayField(TEXT("notes"), RecoveryNotes));
	TestEqual(TEXT("recovery action matches contract"), RecommendedAction, FString(TEXT("inspect_task_result_then_submit_followup_taskspec")));
	TestFalse(TEXT("partial failure journal is not marked safe to retry"), bSafeToRetry);
	TestFalse(TEXT("partial failure journal does not promise rollback"), bRollbackAvailable);
	TestTrue(TEXT("recovery notes array is valid"), RecoveryNotes != nullptr);
	if (RecoveryNotes)
	{
		for (const TSharedPtr<FJsonValue>& RecoveryNote : *RecoveryNotes)
		{
			TestTrue(TEXT("recovery note is string"), RecoveryNote.IsValid() && RecoveryNote->Type == EJson::String);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* JournalSteps = nullptr;
	TestTrue(TEXT("journal exposes all task plan steps"), Journal->TryGetArrayField(TEXT("steps"), JournalSteps));
	TestEqual(TEXT("journal keeps failed, blocked, and independent steps"), JournalSteps ? JournalSteps->Num() : 0, 3);

	const TSharedPtr<FJsonObject> FirstJournalStep = (*JournalSteps)[0]->AsObject();
	const TSharedPtr<FJsonObject> SecondJournalStep = (*JournalSteps)[1]->AsObject();
	const TSharedPtr<FJsonObject> ThirdJournalStep = (*JournalSteps)[2]->AsObject();

	FString FirstStatus;
	FString SecondStatus;
	FString ThirdStatus;
	TestTrue(TEXT("first journal step has status"), FirstJournalStep->TryGetStringField(TEXT("status"), FirstStatus));
	TestTrue(TEXT("second journal step has status"), SecondJournalStep->TryGetStringField(TEXT("status"), SecondStatus));
	TestTrue(TEXT("third journal step has status"), ThirdJournalStep->TryGetStringField(TEXT("status"), ThirdStatus));
	TestEqual(TEXT("failed step remains failed"), FirstStatus, FString(TEXT("failed")));
	TestEqual(TEXT("dependent step is blocked"), SecondStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("independent step still completes"), ThirdStatus, FString(TEXT("completed")));

	const TArray<TSharedPtr<FJsonValue>>* BlockedDependsOn = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BlockedBy = nullptr;
	FString BlockedReason;
	TestTrue(TEXT("blocked step keeps depends_on"), SecondJournalStep->TryGetArrayField(TEXT("depends_on"), BlockedDependsOn));
	TestTrue(TEXT("blocked step records blocked_by_step_ids"), SecondJournalStep->TryGetArrayField(TEXT("blocked_by_step_ids"), BlockedBy));
	TestTrue(TEXT("blocked step records blocked_reason"), SecondJournalStep->TryGetStringField(TEXT("blocked_reason"), BlockedReason));
	TestEqual(TEXT("blocked step keeps one depends_on entry"), BlockedDependsOn ? BlockedDependsOn->Num() : 0, 1);
	TestEqual(TEXT("blocked step keeps one blocking dependency"), BlockedBy ? BlockedBy->Num() : 0, 1);
	TestEqual(TEXT("blocked reason is dependency_failed"), BlockedReason, FString(TEXT("dependency_failed")));
	TestTrue(TEXT("blocked step includes explicit null error field"), SecondJournalStep->HasField(TEXT("error")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeRecordsCompileSavePostOperationsTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeRecordsCompileSavePostOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeRecordsCompileSavePostOperationsTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep());

	FBlueprintHelperTaskRuntimeLoweredStep Step;
	Step.StepId = TEXT("step_001");
	Step.Capability = TEXT("graph_write");
	Step.RuntimeOperation = TEXT("graph_write");
	Step.AdapterOperation = TEXT("append_blueprint_graph");
	FBlueprintHelperToolResultBase StepResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("append_blueprint_graph"),
		TEXT("trace_step"));

	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	StepRecords.Add({Step, StepResult});

	FBlueprintHelperToolResultBase CompileResult = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("compile_blueprint_asset"),
		TEXT("trace_compile"));
	FBlueprintHelperToolResultBase SaveResult = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("save_asset"),
		TEXT("trace_save"));

	TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PostOperations;
	FBlueprintHelperTaskRuntimePostOperationRecord CompilePostOperation;
	CompilePostOperation.Operation = TEXT("compile_blueprint_asset");
	CompilePostOperation.Result = CompileResult;
	CompilePostOperation.AssetPath = TEXT("/Game/Blueprints/BP_StoneGate");
	CompilePostOperation.Status = TEXT("executed");
	CompilePostOperation.DurationMs = 1.25;
	PostOperations.Add(CompilePostOperation);

	FBlueprintHelperTaskRuntimePostOperationRecord SavePostOperation;
	SavePostOperation.Operation = TEXT("save_asset");
	SavePostOperation.Result = SaveResult;
	SavePostOperation.AssetPath = TEXT("/Game/Blueprints/BP_StoneGate");
	SavePostOperation.Status = TEXT("skipped");
	SavePostOperation.Reason = TEXT("package_not_loaded_or_clean");
	SavePostOperation.DurationMs = 0.25;
	PostOperations.Add(SavePostOperation);

	const TSharedRef<FJsonObject> RuntimeData = FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForSteps(
		TaskPlan,
		TEXT("task_compile_save"),
		StepRecords,
		PostOperations,
		false);
	const TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
		TEXT("task_compile_save"),
		TaskPlan,
		StepRecords,
		PostOperations);

	const TArray<TSharedPtr<FJsonValue>>* RuntimePostOps = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JournalPostOps = nullptr;
	TestTrue(TEXT("runtime data exposes post operations"), RuntimeData->TryGetArrayField(TEXT("post_operations"), RuntimePostOps));
	TestTrue(TEXT("journal exposes post operations"), Journal->TryGetArrayField(TEXT("post_operations"), JournalPostOps));
	TestEqual(TEXT("runtime data keeps compile/save results"), RuntimePostOps->Num(), 2);
	TestEqual(TEXT("journal keeps compile/save results"), JournalPostOps->Num(), 2);

	const TSharedPtr<FJsonObject> RuntimeCompile = (*RuntimePostOps)[0]->AsObject();
	const TSharedPtr<FJsonObject> RuntimeSave = (*RuntimePostOps)[1]->AsObject();
	FString CompileOperation;
	FString SaveOperation;
	TestTrue(TEXT("compile post op exposes operation"), RuntimeCompile->TryGetStringField(TEXT("operation"), CompileOperation));
	TestTrue(TEXT("save post op exposes operation"), RuntimeSave->TryGetStringField(TEXT("operation"), SaveOperation));
	TestEqual(TEXT("first post op is compile"), CompileOperation, FString(TEXT("compile_blueprint_asset")));
	TestEqual(TEXT("second post op is save"), SaveOperation, FString(TEXT("save_asset")));
	TestTrue(TEXT("compile post op has asset path"), RuntimeCompile->HasField(TEXT("asset_path")));
	TestTrue(TEXT("compile post op has post status"), RuntimeCompile->HasField(TEXT("post_status")));
	TestTrue(TEXT("save post op has asset path"), RuntimeSave->HasField(TEXT("asset_path")));
	TestTrue(TEXT("save post op has reason field"), RuntimeSave->HasField(TEXT("reason")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrRejectsAdapterOperationFieldTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrRejectsAdapterOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrRejectsAdapterOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep();
	Step->SetStringField(TEXT("operation"), TEXT("append_blueprint_graph"));
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("graph_write IR rejects adapter operation compatibility field"), bLowered);
	TestEqual(TEXT("mixed graph_write IR reports operation field error"), Error.Code, FString(TEXT("unsupported_graph_write_operation_field")));
	TestEqual(TEXT("mixed graph_write IR reports parse_input stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("mixed graph_write IR points at operation field"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableEnsureMemberLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableEnsureMemberLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableEnsureMemberLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_variables"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("bool"));

	TSharedPtr<FJsonObject> Flags = MakeShared<FJsonObject>();
	Flags->SetBoolField(TEXT("expose_on_spawn"), false);

	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), TEXT("ensure_member_variable"));
	Op->SetStringField(TEXT("name"), TEXT("bDoorOpen"));
	Op->SetObjectField(TEXT("pin_type"), PinType);
	Op->SetStringField(TEXT("category"), TEXT("Door"));
	Op->SetObjectField(TEXT("flags"), Flags);

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("member_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetBoolField(TEXT("allow_remove_referenced_variables"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("blueprint_variable ensure_member_variable lowers successfully"), bLowered);
	TestEqual(TEXT("blueprint_variable step reports capability"), LoweredStep.Capability, FString(TEXT("blueprint_variable")));
	TestEqual(TEXT("blueprint_variable runtime operation reports capability"), LoweredStep.RuntimeOperation, FString(TEXT("blueprint_variable")));
	TestEqual(TEXT("blueprint_variable lowers to batch add adapter"), LoweredStep.AdapterOperation, FString(TEXT("add_blueprint_member_variables")));
	TestFalse(TEXT("blueprint_variable preview does not call a mutating adapter dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("lowered variable payload exists"), LoweredStep.Payload.Get());

	FString AssetPath;
	TestTrue(TEXT("variable payload carries asset_path"), LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestEqual(TEXT("variable payload asset_path matches task target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_StoneGate")));

	const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
	TestTrue(TEXT("variable payload carries variables array"), LoweredStep.Payload->TryGetArrayField(TEXT("variables"), Variables));
	TestEqual(TEXT("variable payload contains one variable"), Variables->Num(), 1);

	const TSharedPtr<FJsonObject> Variable = (*Variables)[0]->AsObject();
	FString VariableName;
	TestTrue(TEXT("variable name preserved"), Variable->TryGetStringField(TEXT("name"), VariableName));
	TestEqual(TEXT("variable name matches op"), VariableName, FString(TEXT("bDoorOpen")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableMemberChangesLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableMemberChangesLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableMemberChangesLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_variables"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("bool"));

	TSharedPtr<FJsonObject> EnsureOp = MakeShared<FJsonObject>();
	EnsureOp->SetStringField(TEXT("op"), TEXT("ensure_member_variable"));
	EnsureOp->SetStringField(TEXT("name"), TEXT("bDoorOpen"));
	EnsureOp->SetObjectField(TEXT("pin_type"), PinType);

	TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
	Setting->SetStringField(TEXT("property_path"), TEXT("Tooltip"));
	Setting->SetStringField(TEXT("value"), TEXT("Door open state."));

	TSharedPtr<FJsonObject> ReplicationValue = MakeShared<FJsonObject>();
	ReplicationValue->SetStringField(TEXT("mode"), TEXT("rep_notify"));
	ReplicationValue->SetStringField(TEXT("condition"), TEXT("owner_only"));
	ReplicationValue->SetStringField(TEXT("notify_function"), TEXT("OnRep_DoorOpen"));
	ReplicationValue->SetBoolField(TEXT("create_notify_function"), true);
	ReplicationValue->SetBoolField(TEXT("reuse_existing_notify_function"), false);

	TSharedPtr<FJsonObject> ReplicationSetting = MakeShared<FJsonObject>();
	ReplicationSetting->SetStringField(TEXT("property_path"), TEXT("replication"));
	ReplicationSetting->SetObjectField(TEXT("value"), ReplicationValue);

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(MakeShared<FJsonValueObject>(Setting.ToSharedRef()));
	Settings.Add(MakeShared<FJsonValueObject>(ReplicationSetting.ToSharedRef()));

	TSharedPtr<FJsonObject> ConfigureOp = MakeShared<FJsonObject>();
	ConfigureOp->SetStringField(TEXT("op"), TEXT("set_member_variable_properties"));
	ConfigureOp->SetStringField(TEXT("name"), TEXT("bDoorOpen"));
	ConfigureOp->SetArrayField(TEXT("settings"), Settings);

	TSharedPtr<FJsonObject> RemoveOp = MakeShared<FJsonObject>();
	RemoveOp->SetStringField(TEXT("op"), TEXT("remove_member_variable"));
	RemoveOp->SetStringField(TEXT("name"), TEXT("bDeprecatedDoorOpen"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(EnsureOp.ToSharedRef()));
	Ops.Add(MakeShared<FJsonValueObject>(ConfigureOp.ToSharedRef()));
	Ops.Add(MakeShared<FJsonValueObject>(RemoveOp.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("member_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetBoolField(TEXT("allow_remove_referenced_variables"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("blueprint_variable member changes lower successfully"), bLowered);
	TestEqual(TEXT("member changes lower to internal batch adapter"), LoweredStep.AdapterOperation, FString(TEXT("blueprint_variable_batch")));
	TestFalse(TEXT("member change batch preview uses synthetic dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("member change payload exists"), LoweredStep.Payload.Get());

	FString Strategy;
	bool bDryRun = false;
	const TArray<TSharedPtr<FJsonValue>>* PayloadOps = nullptr;
	TestTrue(TEXT("member change payload carries strategy"), LoweredStep.Payload->TryGetStringField(TEXT("strategy"), Strategy));
	TestTrue(TEXT("member change payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("member change payload carries ops"), LoweredStep.Payload->TryGetArrayField(TEXT("ops"), PayloadOps));
	TestEqual(TEXT("member change strategy preserved"), Strategy, FString(TEXT("member_variables")));
	TestTrue(TEXT("member change dry_run preserved"), bDryRun);
	TestEqual(TEXT("member change op count preserved"), PayloadOps ? PayloadOps->Num() : 0, 3);
	if (PayloadOps && PayloadOps->Num() >= 2)
	{
		const TSharedPtr<FJsonObject> PayloadConfigureOp = (*PayloadOps)[1].IsValid()
			? (*PayloadOps)[1]->AsObject()
			: nullptr;
		const TArray<TSharedPtr<FJsonValue>>* PayloadSettings = nullptr;
		TestTrue(TEXT("configure op keeps settings"),
			PayloadConfigureOp.IsValid() &&
			PayloadConfigureOp->TryGetArrayField(TEXT("settings"), PayloadSettings) &&
			PayloadSettings);
		TestEqual(TEXT("configure op keeps metadata and replication settings"), PayloadSettings ? PayloadSettings->Num() : 0, 2);
		if (PayloadSettings && PayloadSettings->Num() >= 2)
		{
			const TSharedPtr<FJsonObject> PayloadReplicationSetting = (*PayloadSettings)[1].IsValid()
				? (*PayloadSettings)[1]->AsObject()
				: nullptr;
			FString PropertyPath;
			const TSharedPtr<FJsonObject>* PayloadReplicationValue = nullptr;
			TestTrue(TEXT("replication setting path is preserved"),
				PayloadReplicationSetting.IsValid() &&
				PayloadReplicationSetting->TryGetStringField(TEXT("property_path"), PropertyPath));
			TestEqual(TEXT("replication property path"), PropertyPath, FString(TEXT("replication")));
			TestTrue(TEXT("replication setting value is preserved"),
				PayloadReplicationSetting.IsValid() &&
				PayloadReplicationSetting->TryGetObjectField(TEXT("value"), PayloadReplicationValue) &&
				PayloadReplicationValue &&
				PayloadReplicationValue->IsValid());
			if (PayloadReplicationValue && PayloadReplicationValue->IsValid())
			{
				FString Mode;
				FString Condition;
				FString NotifyFunction;
				TestTrue(TEXT("replication mode is preserved"), (*PayloadReplicationValue)->TryGetStringField(TEXT("mode"), Mode));
				TestTrue(TEXT("replication condition is preserved"), (*PayloadReplicationValue)->TryGetStringField(TEXT("condition"), Condition));
				TestTrue(TEXT("replication notify function is preserved"), (*PayloadReplicationValue)->TryGetStringField(TEXT("notify_function"), NotifyFunction));
				TestEqual(TEXT("replication mode value"), Mode, FString(TEXT("rep_notify")));
				TestEqual(TEXT("replication condition value"), Condition, FString(TEXT("owner_only")));
				TestEqual(TEXT("replication notify function value"), NotifyFunction, FString(TEXT("OnRep_DoorOpen")));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableReplicationRejectsHiddenConditionTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableReplicationRejectsHiddenCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableReplicationRejectsHiddenConditionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_variable_replication"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> ReplicationValue = MakeShared<FJsonObject>();
	ReplicationValue->SetStringField(TEXT("mode"), TEXT("replicated"));
	ReplicationValue->SetStringField(TEXT("condition"), TEXT("dynamic"));

	TSharedPtr<FJsonObject> ReplicationSetting = MakeShared<FJsonObject>();
	ReplicationSetting->SetStringField(TEXT("property_path"), TEXT("replication"));
	ReplicationSetting->SetObjectField(TEXT("value"), ReplicationValue);

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(MakeShared<FJsonValueObject>(ReplicationSetting.ToSharedRef()));

	TSharedPtr<FJsonObject> ConfigureOp = MakeShared<FJsonObject>();
	ConfigureOp->SetStringField(TEXT("op"), TEXT("set_member_variable_properties"));
	ConfigureOp->SetStringField(TEXT("name"), TEXT("DoorState"));
	ConfigureOp->SetArrayField(TEXT("settings"), Settings);

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(ConfigureOp.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("member_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("hidden replication condition is rejected"), bLowered);
	TestEqual(TEXT("hidden replication condition error code"), Error.Code, FString(TEXT("invalid_replication_condition")));
	TestEqual(TEXT("hidden replication condition field"),
		Error.Field,
		FString(TEXT("task_plan.steps[0].write.ops[0].settings[0].value.condition")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableMemberDefaultsLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableMemberDefaultsLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableMemberDefaultsLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_variable_defaults"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> DefaultOp = MakeShared<FJsonObject>();
	DefaultOp->SetStringField(TEXT("op"), TEXT("set_member_default"));
	DefaultOp->SetStringField(TEXT("name"), TEXT("Health"));
	DefaultOp->SetField(TEXT("value"), MakeShared<FJsonValueNumber>(100.0));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(DefaultOp.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("member_defaults"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetBoolField(TEXT("allow_remove_referenced_variables"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("blueprint_variable member defaults lower successfully"), bLowered);
	TestEqual(TEXT("member defaults lower to internal batch adapter"), LoweredStep.AdapterOperation, FString(TEXT("blueprint_variable_batch")));
	TestNotNull(TEXT("member defaults payload exists"), LoweredStep.Payload.Get());

	FString Strategy;
	const TArray<TSharedPtr<FJsonValue>>* PayloadOps = nullptr;
	TestTrue(TEXT("member defaults payload carries strategy"), LoweredStep.Payload->TryGetStringField(TEXT("strategy"), Strategy));
	TestTrue(TEXT("member defaults payload carries ops"), LoweredStep.Payload->TryGetArrayField(TEXT("ops"), PayloadOps));
	TestEqual(TEXT("member defaults strategy preserved"), Strategy, FString(TEXT("member_defaults")));
	TestEqual(TEXT("member defaults op count preserved"), PayloadOps ? PayloadOps->Num() : 0, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableLocalVariablesLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableLocalVariablesLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableLocalVariablesLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_local_variables"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Target->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("float"));

	TSharedPtr<FJsonObject> EnsureOp = MakeShared<FJsonObject>();
	EnsureOp->SetStringField(TEXT("op"), TEXT("ensure_local_variable"));
	EnsureOp->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	EnsureOp->SetStringField(TEXT("name"), TEXT("DamageScale"));
	EnsureOp->SetObjectField(TEXT("pin_type"), PinType);

	TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
	Setting->SetStringField(TEXT("property_path"), TEXT("Tooltip"));
	Setting->SetStringField(TEXT("value"), TEXT("Current damage scale."));

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(MakeShared<FJsonValueObject>(Setting.ToSharedRef()));

	TSharedPtr<FJsonObject> ConfigureOp = MakeShared<FJsonObject>();
	ConfigureOp->SetStringField(TEXT("op"), TEXT("set_local_variable_properties"));
	ConfigureOp->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	ConfigureOp->SetStringField(TEXT("name"), TEXT("DamageScale"));
	ConfigureOp->SetArrayField(TEXT("settings"), Settings);

	TSharedPtr<FJsonObject> RemoveOp = MakeShared<FJsonObject>();
	RemoveOp->SetStringField(TEXT("op"), TEXT("remove_local_variable"));
	RemoveOp->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	RemoveOp->SetStringField(TEXT("name"), TEXT("OldDamageScale"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(EnsureOp.ToSharedRef()));
	Ops.Add(MakeShared<FJsonValueObject>(ConfigureOp.ToSharedRef()));
	Ops.Add(MakeShared<FJsonValueObject>(RemoveOp.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("local_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetBoolField(TEXT("allow_remove_referenced_variables"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("blueprint_variable local variables lower successfully"), bLowered);
	TestEqual(TEXT("local variables lower to internal batch adapter"), LoweredStep.AdapterOperation, FString(TEXT("blueprint_variable_batch")));
	TestNotNull(TEXT("local variable payload exists"), LoweredStep.Payload.Get());

	FString Strategy;
	FString FunctionName;
	const TArray<TSharedPtr<FJsonValue>>* PayloadOps = nullptr;
	TestTrue(TEXT("local variable payload carries strategy"), LoweredStep.Payload->TryGetStringField(TEXT("strategy"), Strategy));
	TestTrue(TEXT("local variable payload carries function_name"), LoweredStep.Payload->TryGetStringField(TEXT("function_name"), FunctionName));
	TestTrue(TEXT("local variable payload carries ops"), LoweredStep.Payload->TryGetArrayField(TEXT("ops"), PayloadOps));
	TestEqual(TEXT("local variable strategy preserved"), Strategy, FString(TEXT("local_variables")));
	TestEqual(TEXT("local variable function_name preserved"), FunctionName, FString(TEXT("CalculateDamage")));
	TestEqual(TEXT("local variable op count preserved"), PayloadOps ? PayloadOps->Num() : 0, 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeBlueprintVariableLocalVariablesRequireFunctionNameTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeBlueprintVariableLocalVariablesRequireFunctionName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeBlueprintVariableLocalVariablesRequireFunctionNameTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_local_variables"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("float"));

	TSharedPtr<FJsonObject> EnsureOp = MakeShared<FJsonObject>();
	EnsureOp->SetStringField(TEXT("op"), TEXT("ensure_local_variable"));
	EnsureOp->SetStringField(TEXT("name"), TEXT("DamageScale"));
	EnsureOp->SetObjectField(TEXT("pin_type"), PinType);

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(EnsureOp.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("local_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("local variable lowering rejects missing function_name"), bLowered);
	TestEqual(TEXT("missing function_name reports target error"), Error.Code, FString(TEXT("invalid_taskplan_step_target")));
	TestEqual(TEXT("missing function_name reports parse_input stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("missing function_name points at target.function_name"), Error.Field, FString(TEXT("task_plan.steps[0].target.function_name")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrUnsupportedOpTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrUnsupportedOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrUnsupportedOpTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteEnsureEntryStep(false);
	const TSharedPtr<FJsonObject>* WriteObject = nullptr;
	TestTrue(TEXT("graph_write step exposes write object"), Step->TryGetObjectField(TEXT("write"), WriteObject));

	const TArray<TSharedPtr<FJsonValue>>* ExistingOps = nullptr;
	TestTrue(TEXT("graph_write step exposes ops array"), (*WriteObject)->TryGetArrayField(TEXT("ops"), ExistingOps));
	TArray<TSharedPtr<FJsonValue>> Ops = ExistingOps ? *ExistingOps : TArray<TSharedPtr<FJsonValue>>();

	TSharedPtr<FJsonObject> UnsupportedOp = MakeShared<FJsonObject>();
	UnsupportedOp->SetStringField(TEXT("op"), TEXT("set_pin_default"));
	Ops.Add(MakeShared<FJsonValueObject>(UnsupportedOp.ToSharedRef()));
	(*WriteObject)->SetArrayField(TEXT("ops"), Ops);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("unsupported graph_write IR op is rejected"), bLowered);
	TestEqual(TEXT("unsupported op reports graph_write IR error code"), Error.Code, FString(TEXT("unsupported_graph_write_ir_op")));
	TestEqual(TEXT("unsupported op reports parse_input stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("unsupported op points at write.ops entry"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[1].op")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrReplaceLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrReplaceLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrReplaceLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Selector = MakeShared<FJsonObject>();
	Selector->SetStringField(TEXT("entry_name"), TEXT("ToggleDoor"));
	Selector->SetStringField(TEXT("node_path"), TEXT("logic.groups[0].entry.node_path"));

	TSharedPtr<FJsonObject> Replacement = MakeShared<FJsonObject>();
	Replacement->SetArrayField(TEXT("nodes"), {});
	Replacement->SetArrayField(TEXT("links"), {});

	TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("strict"), true);
	Options->SetBoolField(TEXT("preserve_layout"), false);

	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), TEXT("replace_body"));
	Op->SetStringField(TEXT("replace_scope"), TEXT("custom_event_body"));
	Op->SetObjectField(TEXT("selector"), Selector);
	Op->SetObjectField(TEXT("replacement"), Replacement);
	Op->SetObjectField(TEXT("options"), Options);

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteStepWithSingleOp(Op);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("replace_body lowers successfully"), bLowered);
	TestEqual(TEXT("replace_body lowers to replace adapter"), LoweredStep.AdapterOperation, FString(TEXT("replace_blueprint_graph")));

	const TSharedPtr<FJsonObject>* Target = nullptr;
	TestTrue(TEXT("replace payload has target"), LoweredStep.Payload->TryGetObjectField(TEXT("target"), Target));
	FString ReplaceScope;
	TestTrue(TEXT("replace target has replace_scope"), (*Target)->TryGetStringField(TEXT("replace_scope"), ReplaceScope));
	TestEqual(TEXT("replace_scope preserved"), ReplaceScope, FString(TEXT("custom_event_body")));

	const TSharedPtr<FJsonObject>* PayloadOptions = nullptr;
	TestTrue(TEXT("replace payload has options"), LoweredStep.Payload->TryGetObjectField(TEXT("options"), PayloadOptions));
	bool bDryRun = false;
	TestTrue(TEXT("replace options inject dry_run"), (*PayloadOptions)->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("replace dry_run=true"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrPatchLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrPatchLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrPatchLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
	PatchedRef->SetStringField(TEXT("node_ref"), TEXT("Branch0"));
	PatchedRef->SetStringField(TEXT("pin_ref"), TEXT("Condition"));

	TSharedPtr<FJsonObject> Patch = MakeShared<FJsonObject>();
	Patch->SetBoolField(TEXT("value"), true);

	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), TEXT("set_pin_default"));
	Op->SetStringField(TEXT("patch_scope"), TEXT("pin_default"));
	Op->SetObjectField(TEXT("patched_ref"), PatchedRef);
	Op->SetObjectField(TEXT("patch"), Patch);

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteStepWithSingleOp(Op);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("set_pin_default lowers successfully"), bLowered);
	TestEqual(TEXT("set_pin_default lowers to patch adapter"), LoweredStep.AdapterOperation, FString(TEXT("patch_blueprint_graph")));

	FString PatchType;
	TestTrue(TEXT("patch payload has patch_type"), LoweredStep.Payload->TryGetStringField(TEXT("patch_type"), PatchType));
	TestEqual(TEXT("patch_type preserved"), PatchType, FString(TEXT("set_pin_default")));

	bool bDryRun = false;
	TestTrue(TEXT("patch payload injects dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("patch dry_run=true"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeGraphWriteIrMergeLoweringTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeGraphWriteIrMergeLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeGraphWriteIrMergeLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Anchor = MakeShared<FJsonObject>();
	Anchor->SetStringField(TEXT("node_ref"), TEXT("BeginPlay0"));
	Anchor->SetStringField(TEXT("pin_ref"), TEXT("Then"));

	TSharedPtr<FJsonObject> Inserted = MakeShared<FJsonObject>();
	Inserted->SetStringField(TEXT("block_id"), TEXT("BH_DoorFeature_ToggleDoor"));
	Inserted->SetStringField(TEXT("block_ref"), TEXT("block:BH_DoorFeature_ToggleDoor"));

	TArray<TSharedPtr<FJsonValue>> SequenceOrder;
	SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("BeginPlay0")));
	SequenceOrder.Add(MakeShared<FJsonValueString>(TEXT("BH_DoorFeature_ToggleDoor")));

	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), TEXT("insert_flow"));
	Op->SetStringField(TEXT("merge_scope"), TEXT("owned_block_call"));
	Op->SetStringField(TEXT("insert_strategy"), TEXT("insert_between"));
	Op->SetObjectField(TEXT("anchor"), Anchor);
	Op->SetObjectField(TEXT("inserted"), Inserted);
	Op->SetArrayField(TEXT("sequence_order"), SequenceOrder);

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteStepWithSingleOp(Op);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("insert_flow lowers successfully"), bLowered);
	TestEqual(TEXT("insert_flow lowers to merge adapter"), LoweredStep.AdapterOperation, FString(TEXT("merge_blueprint_graph")));

	const TSharedPtr<FJsonObject>* Target = nullptr;
	TestTrue(TEXT("merge payload has target"), LoweredStep.Payload->TryGetObjectField(TEXT("target"), Target));
	FString MergeScope;
	FString InsertStrategy;
	TestTrue(TEXT("merge target has merge_scope"), (*Target)->TryGetStringField(TEXT("merge_scope"), MergeScope));
	TestTrue(TEXT("merge target has insert_strategy"), (*Target)->TryGetStringField(TEXT("insert_strategy"), InsertStrategy));
	TestEqual(TEXT("merge_scope preserved"), MergeScope, FString(TEXT("owned_block_call")));
	TestEqual(TEXT("insert_strategy preserved"), InsertStrategy, FString(TEXT("insert_between")));

	bool bDryRun = false;
	TestTrue(TEXT("merge payload injects dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("merge dry_run=true"), bDryRun);

	bool bAllowCompileBeforeCall = false;
	TestTrue(TEXT("merge payload carries allow_compile_before_call"),
		LoweredStep.Payload->TryGetBoolField(TEXT("allow_compile_before_call"), bAllowCompileBeforeCall));
	TestTrue(TEXT("allow_compile_before_call follows execution policy"), bAllowCompileBeforeCall);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractTaskRuntimeLegacyAppendCompatibilityTest,
	"BlueprintHelper.ObjectFirst.Contract.TaskRuntimeLegacyAppendCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractTaskRuntimeLegacyAppendCompatibilityTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_legacy"));
	Step->SetStringField(TEXT("operation"), TEXT("append_blueprint_graph"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_StoneGate"));
	Target->SetStringField(TEXT("graph"), TEXT("BH_Legacy"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> EntryNode = MakeShared<FJsonObject>();
	EntryNode->SetStringField(TEXT("id"), TEXT("Legacy_entry"));
	EntryNode->SetStringField(TEXT("kind"), TEXT("custom_event"));
	EntryNode->SetStringField(TEXT("name"), TEXT("LegacyEntry"));

	TSharedPtr<FJsonObject> CallNode = MakeShared<FJsonObject>();
	CallNode->SetStringField(TEXT("id"), TEXT("Legacy_stmt_1"));
	CallNode->SetStringField(TEXT("kind"), TEXT("call"));
	CallNode->SetStringField(TEXT("function"), TEXT("PrintString"));
	TSharedPtr<FJsonObject> Inputs = MakeShared<FJsonObject>();
	Inputs->SetStringField(TEXT("InString"), TEXT("legacy"));
	CallNode->SetObjectField(TEXT("inputs"), Inputs);

	TArray<TSharedPtr<FJsonValue>> Nodes;
	Nodes.Add(MakeShared<FJsonValueObject>(EntryNode.ToSharedRef()));
	Nodes.Add(MakeShared<FJsonValueObject>(CallNode.ToSharedRef()));

	TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
	Link->SetStringField(TEXT("kind"), TEXT("exec"));
	Link->SetStringField(TEXT("from"), TEXT("Legacy_entry.then"));
	Link->SetStringField(TEXT("to"), TEXT("Legacy_stmt_1.execute"));
	TArray<TSharedPtr<FJsonValue>> Links;
	Links.Add(MakeShared<FJsonValueObject>(Link.ToSharedRef()));

	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("feature_name"), TEXT("LegacyFeature"));
	Args->SetArrayField(TEXT("nodes"), Nodes);
	Args->SetArrayField(TEXT("links"), Links);
	Step->SetObjectField(TEXT("args"), Args);

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperObjectFirstContractTestsLocalUtils::MakeGraphWriteTaskPlan(Step);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("legacy append adapter step still lowers"), bLowered);
	TestEqual(TEXT("legacy append runtime operation stays append"), LoweredStep.RuntimeOperation, FString(TEXT("append_blueprint_graph")));
	TestEqual(TEXT("legacy append adapter operation stays append"), LoweredStep.AdapterOperation, FString(TEXT("append_blueprint_graph")));
	TestEqual(TEXT("legacy append step has no graph_write capability"), LoweredStep.Capability, FString());

	bool bDryRun = true;
	TestTrue(TEXT("legacy append payload injects execute dry_run=false"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestFalse(TEXT("execute path sets dry_run=false"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractReadReferenceContextPayloadTest,
	"BlueprintHelper.ObjectFirst.Contract.ReadReferenceContextPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractReadReferenceContextPayloadTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));

		TestTrue(TEXT("read_reference_context accepts a minimal asset target"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

		TestFalse(TEXT("read_reference_context rejects missing asset_path"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("missing asset_path error field"), Error.Field, FString(TEXT("payload.asset_path")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("target_type"), TEXT("unsupported_target"));

		TestFalse(TEXT("read_reference_context rejects invalid target_type"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("invalid target_type error field"), Error.Field, FString(TEXT("payload.target_type")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("target_type"), TEXT("custom_event"));
		Payload->SetStringField(TEXT("target_name"), TEXT("ToggleDoor"));
		Payload->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
		Payload->SetStringField(TEXT("search_scope"), TEXT("project"));
		Payload->SetStringField(TEXT("resolution_policy"), TEXT("ue_then_name"));
		Payload->SetStringField(TEXT("detail"), TEXT("samples"));
		Payload->SetNumberField(TEXT("max_results"), 25);

		TestTrue(TEXT("read_reference_context accepts scoped custom_event request"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("target_type"), TEXT("local_variable"));
		Payload->SetStringField(TEXT("target_name"), TEXT("LoopIndex"));
		Payload->SetStringField(TEXT("graph_name"), TEXT("BuildDoor"));
		Payload->SetStringField(TEXT("search_scope"), TEXT("asset"));
		Payload->SetStringField(TEXT("resolution_policy"), TEXT("ue_only"));
		Payload->SetStringField(TEXT("detail"), TEXT("summary"));

		TestTrue(TEXT("read_reference_context accepts local_variable request"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("target_type"), TEXT("function"));
		Payload->SetStringField(TEXT("search_scope"), TEXT("project"));

		TestFalse(TEXT("read_reference_context rejects member target without target_name"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("missing target_name error field"), Error.Field, FString(TEXT("payload.target_name")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("target_type"), TEXT("function"));
		Payload->SetStringField(TEXT("target_name"), TEXT("ToggleDoor"));
		Payload->SetStringField(TEXT("target_guid"), TEXT("00000000000000000000000000000000"));

		TestFalse(TEXT("read_reference_context rejects target_guid"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("target_guid error field"), Error.Field, FString(TEXT("payload.target_guid")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetBoolField(TEXT("include_samples"), true);

		TestFalse(TEXT("read_reference_context rejects include_samples"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("include_samples error field"), Error.Field, FString(TEXT("payload.include_samples")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_Door"));
		Payload->SetStringField(TEXT("scope"), TEXT("all"));

		TestFalse(TEXT("read_reference_context rejects legacy scope"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_reference_context"), Payload, Error));
		TestEqual(TEXT("scope error field"), Error.Field, FString(TEXT("payload.scope")));
	}

	TestFalse(TEXT("read_reference_context is not a write command"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("read_reference_context")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReferenceContextPackShapeTest,
	"BlueprintHelper.ObjectFirst.Contract.ReferenceContextPackShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReferenceContextPackShapeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReferenceContextPack Pack;
	Pack.ContextId = TEXT("refctx_contract");
	Pack.Summary.AssetCount = 1;
	Pack.Summary.ReferenceCount = 2;
	Pack.Summary.BlockingCount = 1;
	Pack.Summary.bPartial = false;
	Pack.Summary.bTruncated = false;
	Pack.IndexStatus.UnindexedCount = 0;
	Pack.IndexStatus.OutOfDateCount = 0;
	Pack.IndexStatus.FailedCount = 0;

	FBlueprintHelperReferenceAssetSummary Referencer;
	Referencer.AssetPath = TEXT("/Game/BP/BP_DoorUser.BP_DoorUser");
	Referencer.AssetType = TEXT("Blueprint");
	Referencer.AddReference(TEXT("function_call"), TEXT("EventGraph"), TEXT("blocking"), true, 3);
	Referencer.AddReference(TEXT("function_call"), TEXT("DoorLogic"), TEXT("blocking"), true, 3);
	Pack.Referencers.Add(Referencer);
	Pack.AgentHints.bCanEditSafely = false;
	Pack.AgentHints.Blockers.Add(TEXT("1 assets have blocking references."));

	const TSharedRef<FJsonObject> Json = Pack.ToJson();
	FString Schema;
	TestTrue(TEXT("reference context pack has schema"), Json->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("reference context pack schema has no BlueprintHelper prefix"), Schema, FString(TEXT("ReferenceContextPack.v1")));
	TestFalse(TEXT("reference context pack does not echo target"), Json->HasField(TEXT("target")));
	TestFalse(TEXT("reference context pack does not echo query"), Json->HasField(TEXT("query")));
	TestFalse(TEXT("reference context pack omits legacy analysis block"), Json->HasField(TEXT("analysis")));
	TestFalse(TEXT("reference context pack omits legacy external_dependents block"), Json->HasField(TEXT("external_dependents")));

	const TSharedPtr<FJsonObject>* SummaryObject = nullptr;
	TestTrue(TEXT("summary exists"), Json->TryGetObjectField(TEXT("summary"), SummaryObject));
	if (SummaryObject && SummaryObject->IsValid())
	{
		TestTrue(TEXT("summary carries partial"), (*SummaryObject)->HasField(TEXT("partial")));
		TestTrue(TEXT("summary carries truncated"), (*SummaryObject)->HasField(TEXT("truncated")));
	}

	const TArray<TSharedPtr<FJsonValue>>* ReferencerValues = nullptr;
	TestTrue(TEXT("referencers exists"), Json->TryGetArrayField(TEXT("referencers"), ReferencerValues));
	if (ReferencerValues && ReferencerValues->Num() > 0)
	{
		const TSharedPtr<FJsonObject> ReferencerJson = (*ReferencerValues)[0]->AsObject();
		TestTrue(TEXT("referencer is asset-level aggregate"), ReferencerJson->HasField(TEXT("match_count")));
		TestTrue(TEXT("referencer has reference kinds"), ReferencerJson->HasField(TEXT("reference_kinds")));
		const TArray<TSharedPtr<FJsonValue>>* SampleValues = nullptr;
		TestTrue(TEXT("referencer has compact samples"), ReferencerJson->TryGetArrayField(TEXT("samples"), SampleValues));
		if (SampleValues && SampleValues->Num() > 0)
		{
			const TSharedPtr<FJsonObject> SampleJson = (*SampleValues)[0]->AsObject();
			TestTrue(TEXT("sample has graph name"), SampleJson->HasField(TEXT("graph_name")));
			TestTrue(TEXT("sample has reference kind"), SampleJson->HasField(TEXT("reference_kind")));
			TestFalse(TEXT("sample does not expose node guid"), SampleJson->HasField(TEXT("node_guid")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractReadFunctionChainContextPayloadTest,
	"BlueprintHelper.ObjectFirst.Contract.ReadFunctionChainContextPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractReadFunctionChainContextPayloadTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_PlayerController"));
		Payload->SetStringField(TEXT("target_type"), TEXT("event"));
		Payload->SetStringField(TEXT("target_name"), TEXT("Input_Fire"));

		TestTrue(TEXT("read_function_chain_context accepts minimal event entry"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_function_chain_context"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_PlayerController"));
		Payload->SetStringField(TEXT("target_type"), TEXT("function"));
		Payload->SetStringField(TEXT("target_name"), TEXT("TryFire"));
		Payload->SetNumberField(TEXT("max_depth"), 4);
		Payload->SetBoolField(TEXT("include_data_dependencies"), true);
		Payload->SetBoolField(TEXT("expand_cross_asset"), true);

		TestTrue(TEXT("read_function_chain_context accepts all supported options"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_function_chain_context"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_PlayerController"));
		Payload->SetStringField(TEXT("target_type"), TEXT("blueprint"));
		Payload->SetStringField(TEXT("target_name"), TEXT("Input_Fire"));

		TestFalse(TEXT("read_function_chain_context rejects unsupported target_type"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_function_chain_context"), Payload, Error));
		TestEqual(TEXT("invalid function chain target_type field"), Error.Field, FString(TEXT("payload.target_type")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_PlayerController"));
		Payload->SetStringField(TEXT("target_type"), TEXT("event"));
		Payload->SetStringField(TEXT("target_name"), TEXT("Input_Fire"));
		Payload->SetStringField(TEXT("target_guid"), TEXT("00000000000000000000000000000000"));

		TestFalse(TEXT("read_function_chain_context rejects target_guid"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("read_function_chain_context"), Payload, Error));
		TestEqual(TEXT("function chain target_guid error field"), Error.Field, FString(TEXT("payload.target_guid")));
	}

	TestFalse(TEXT("read_function_chain_context is not a write command"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("read_function_chain_context")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextPackShapeTest,
	"BlueprintHelper.ObjectFirst.Contract.FunctionChainContextPackShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextPackShapeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFunctionChainContextPack Pack;

	FBlueprintHelperFunctionChainLogicRef Ref;
	Ref.Order = 1;
	Ref.Depth = 1;
	Ref.ParentOrder = 0;
	Ref.AssetPath = TEXT("/Game/BP_Weapon");
	Ref.TargetType = TEXT("function");
	Ref.TargetName = TEXT("CanFire");
	Ref.GraphName = TEXT("CanFire");
	Ref.CallKind = TEXT("pure_function");
	Ref.Reason = TEXT("branch_condition");
	Pack.CustomLogicRefs.Add(Ref);
	Pack.Summary.VisitedNodes = 3;
	Pack.Summary.ReturnedCustomRefs = 1;
	Pack.Summary.FilteredEngineOrTrustedPluginCalls = 2;

	const TSharedRef<FJsonObject> Json = Pack.ToJson();
	FString Schema;
	TestTrue(TEXT("function chain context pack has schema"), Json->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("function chain schema is expected"), Schema, FString(TEXT("FunctionChainContext.v1")));
	TestFalse(TEXT("function chain pack does not echo entry"), Json->HasField(TEXT("entry")));
	TestFalse(TEXT("function chain pack does not echo target"), Json->HasField(TEXT("target")));
	TestFalse(TEXT("function chain pack does not echo query"), Json->HasField(TEXT("query")));
	TestFalse(TEXT("function chain pack does not expose owner"), Json->HasField(TEXT("owner")));

	const TArray<TSharedPtr<FJsonValue>>* RefValues = nullptr;
	TestTrue(TEXT("function chain refs exist"), Json->TryGetArrayField(TEXT("custom_logic_refs"), RefValues));
	if (RefValues && RefValues->Num() > 0)
	{
		const TSharedPtr<FJsonObject> RefJson = (*RefValues)[0]->AsObject();
		TestTrue(TEXT("ref has pure function call kind"), RefJson->HasField(TEXT("call_kind")));
		TestFalse(TEXT("ref has no owner asset path"), RefJson->HasField(TEXT("owner_asset_path")));
		TestFalse(TEXT("ref has no node ref"), RefJson->HasField(TEXT("node_ref")));
		TestFalse(TEXT("ref has no node path"), RefJson->HasField(TEXT("node_path")));
		TestFalse(TEXT("ref has no node guid"), RefJson->HasField(TEXT("node_guid")));
	}

	return true;
}

#endif
