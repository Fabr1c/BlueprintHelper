#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/BlueprintHelperRequestValidator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Services/BlueprintHelperTaskRuntimeService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractExportIncludeJsonTextTest,
	"BlueprintHelper.ObjectFirst.Contract.ExportIncludeJsonText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractExportIncludeJsonTextTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	// 无 include_json_text 的请求应该被接受
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("target_blueprint"), TEXT("/Game/BP/Test"));
		Payload->SetStringField(TEXT("target_graph"), TEXT("EventGraph"));
		Payload->SetStringField(TEXT("scope"), TEXT("graph"));

		TestTrue(TEXT("export_to_json 不带 include_json_text 被接受"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text: true 应该被接受
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetBoolField(TEXT("include_json_text"), true);

		TestTrue(TEXT("export_to_json 接受 include_json_text: true"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text: false 应该被接受
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetBoolField(TEXT("include_json_text"), false);

		TestTrue(TEXT("export_to_json 接受 include_json_text: false"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	// include_json_text 为 string 类型应被拒绝
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("include_json_text"), TEXT("not_a_bool"));

		TestFalse(TEXT("export_to_json 拒绝 string 类型的 include_json_text"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("export_to_json"), Payload, Error));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractImportJsonObjectTest,
	"BlueprintHelper.ObjectFirst.Contract.ImportJsonObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractImportJsonObjectTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	// import_json 接受 object 类型的 json
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("json"), MakeShared<FJsonObject>());

		TestTrue(TEXT("import_json 接受 object 类型的 json"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	}

	// import_json 接受 string 类型的 json（兼容旧格式）
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("json"), TEXT("{\"version\":\"2.2\",\"nodes\":[]}"));

		TestTrue(TEXT("import_json 接受 string 类型的 json"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	}

	// import_json 拒绝 array 类型的 json
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> DummyArray;
		Payload->SetArrayField(TEXT("json"), DummyArray);

		TestFalse(TEXT("import_json 拒绝 array 类型的 json"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	}

	// import_json 依然要求 json 字段存在
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

		TestFalse(TEXT("import_json 拒绝缺少 json 字段的请求"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperContractImportJsonNumberRejectedTest,
	"BlueprintHelper.ObjectFirst.Contract.ImportJsonNumberRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperContractImportJsonNumberRejectedTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperBridgeValidationError Error;

	// import_json 拒绝 number 类型的 json
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetNumberField(TEXT("json"), 42.0);

		TestFalse(TEXT("import_json 拒绝 number 类型的 json"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	}

	// import_json 拒绝 bool 类型的 json
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetBoolField(TEXT("json"), true);

		TestFalse(TEXT("import_json 拒绝 bool 类型的 json"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
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

		TestTrue(TEXT("preview_task_plan 接受 task_plan object"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("preview_task_plan"), Payload, Error));
		TestTrue(TEXT("execute_task_plan 接受 task_plan object"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("execute_task_plan"), Payload, Error));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

		TestFalse(TEXT("preview_task_plan 拒绝缺少 task_plan"),
			FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("preview_task_plan"), Payload, Error));
		TestEqual(TEXT("preview_task_plan 缺失字段定位到 payload.task_plan"),
			Error.Field, FString(TEXT("payload.task_plan")));
	}

	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("task_run_id"), TEXT("task_001"));

		TestTrue(TEXT("get_task_run_journal 接受 task_run_id"),
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
	TestFalse(TEXT("preview_task_plan 不是写命令"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("preview_task_plan")));
	TestTrue(TEXT("execute_task_plan 是写命令"),
		FBlueprintHelperRequestValidator::IsWriteCommand(TEXT("execute_task_plan")));
	TestFalse(TEXT("get_task_run_journal 不是写命令"),
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

	TestFalse(TEXT("TaskRuntime 使用 execution_policy.should_compile"),
		RuntimeValidation.bShouldCompile);
	TestFalse(TEXT("TaskRuntime 使用 execution_policy.should_save"),
		RuntimeValidation.bShouldSave);

	TSharedPtr<FJsonObject> TaskPlanWithoutPolicy = MakeShared<FJsonObject>();
	const FBlueprintHelperValidationSummary FallbackValidation =
		FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(TaskPlanWithoutPolicy, BaseValidation);

	TestTrue(TEXT("缺少 execution_policy 时保留基础 should_compile"),
		FallbackValidation.bShouldCompile);
	TestTrue(TEXT("缺少 execution_policy 时保留基础 should_save"),
		FallbackValidation.bShouldSave);

	return true;
}

#endif
