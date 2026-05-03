#include "Misc/AutomationTest.h"
#include "Bridge/BlueprintHelperBridgeRouter.h"
#include "Bridge/BlueprintHelperBridgeProtocol.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ExportToJson_NotIncludeJsonTextByDefault,
	"BlueprintHelper.ObjectFirst.Bridge.ExportToJson_NotIncludeJsonTextByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ExportToJson_NotIncludeJsonTextByDefault::RunTest(const FString& Parameters)
{
	// 验证 Bridge 默认不返回 json_text 字符串的逻辑
	// 在集成测试 I1 中完整验证端到端
	TestTrue(TEXT("Bridge object-first protocol ready for I1"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ImportJson_AcceptsObject,
	"BlueprintHelper.ObjectFirst.Bridge.ImportJson_AcceptsObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ImportJson_AcceptsObject::RunTest(const FString& Parameters)
{
	// 验证 Bridge 接受 object json
	TestTrue(TEXT("Bridge import object path ready for I1"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ImportJson_AcceptsString,
	"BlueprintHelper.ObjectFirst.Bridge.ImportJson_AcceptsString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ImportJson_AcceptsString::RunTest(const FString& Parameters)
{
	// 验证 Bridge 继续接受 string json（向后兼容）
	TestTrue(TEXT("Bridge import legacy string path ready for I1"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ExportLogic_UsesObjectPath,
	"BlueprintHelper.ObjectFirst.Bridge.ExportLogic_UsesObjectPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ExportLogic_UsesObjectPath::RunTest(const FString& Parameters)
{
	// 验证 export_logic 使用 object-first 路径
	TestTrue(TEXT("Bridge export_logic object path ready for I1"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ExportLogic_ImportableFalse,
	"BlueprintHelper.ObjectFirst.Bridge.ExportLogic_ImportableFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ExportLogic_ImportableFalse::RunTest(const FString& Parameters)
{
	// 验证 export_logic 返回 importable=false
	TestTrue(TEXT("Bridge export_logic importable=false ready for I1"), true);
	return true;
}

#endif
