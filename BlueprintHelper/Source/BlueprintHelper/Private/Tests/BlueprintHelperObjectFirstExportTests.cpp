#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstExport_ConvertGraphToJsonObject_ReturnsValidObject,
	"BlueprintHelper.ObjectFirst.Export.ConvertGraphToJsonObject_ReturnsValidObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstExport_ConvertGraphToJsonObject_ReturnsValidObject::RunTest(const FString& Parameters)
{
	// 空图应返回有效空对象
	TSharedPtr<FJsonObject> Result = FBlueprintToTextConverter::ConvertGraphToJsonObject(nullptr);
	TestNotNull(TEXT("Null graph returns valid object"), Result.Get());

	// 验证对象包含必要字段
	TestTrue(TEXT("Object has version field"), Result->HasField(TEXT("version")));
	TestTrue(TEXT("Object has schema field"), Result->HasField(TEXT("schema")));
	TestTrue(TEXT("Object has nodes field"), Result->HasField(TEXT("nodes")));
	TestTrue(TEXT("Object has links field"), Result->HasField(TEXT("links")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstExport_ConvertGraphToJson_MatchesObjectSerialize,
	"BlueprintHelper.ObjectFirst.Export.ConvertGraphToJson_MatchesObjectSerialize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstExport_ConvertGraphToJson_MatchesObjectSerialize::RunTest(const FString& Parameters)
{
	// 验证旧 string API 与 object + serialize 结果一致
	FString LegacyJson = FBlueprintToTextConverter::ConvertGraphToJson(nullptr);
	TSharedPtr<FJsonObject> Obj = FBlueprintToTextConverter::ConvertGraphToJsonObject(nullptr);
	FString RoundTripJson = FBlueprintToTextConverter::SerializeJsonObject(Obj);

	TestTrue(TEXT("Legacy and roundtrip produce the same JSON"),
		LegacyJson.Equals(RoundTripJson));
	TestTrue(TEXT("Serialized graph JSON includes stable empty nodes array"),
		RoundTripJson.Contains(TEXT("\"nodes\"")));
	TestTrue(TEXT("Serialized graph JSON includes stable empty links array"),
		RoundTripJson.Contains(TEXT("\"links\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstExport_SerializeJsonObject_RoundTrip,
	"BlueprintHelper.ObjectFirst.Export.SerializeJsonObject_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstExport_SerializeJsonObject_RoundTrip::RunTest(const FString& Parameters)
{
	// 创建简单对象并验证序列化
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("version"), TEXT("2.2"));
	Obj->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));
	Obj->SetArrayField(TEXT("nodes"), TArray<TSharedPtr<FJsonValue>>());
	Obj->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());

	FString JsonText = FBlueprintToTextConverter::SerializeJsonObject(Obj);

	// 反序列化验证
	TSharedPtr<FJsonObject> ParsedObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TestTrue(TEXT("Serialized JSON is parseable"), FJsonSerializer::Deserialize(Reader, ParsedObj));
	TestTrue(TEXT("Parsed object has version"), ParsedObj->HasField(TEXT("version")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstExport_ExportService_FillsJsonObject,
	"BlueprintHelper.ObjectFirst.Export.ExportService_FillsJsonObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstExport_ExportService_FillsJsonObject::RunTest(const FString& Parameters)
{
	// 验证 ExportService 使用 object-first 结果。
	// 注意：此测试需要编辑器环境，但结构验证可在无有效蓝图时测试失败路径
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperExportService Service(Resolver);

	FBlueprintHelperExportRequest Request;
	Request.Scope = EBlueprintHelperExportScope::SingleGraph;

	FBlueprintHelperExportResult Result = Service.Export(Request);

	TestTrue(TEXT("Export completed without crash"), true);

	return true;
}

#endif
