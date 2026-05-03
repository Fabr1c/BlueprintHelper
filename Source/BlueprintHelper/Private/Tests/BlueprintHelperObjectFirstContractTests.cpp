#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/BlueprintHelperRequestValidator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

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

#endif
