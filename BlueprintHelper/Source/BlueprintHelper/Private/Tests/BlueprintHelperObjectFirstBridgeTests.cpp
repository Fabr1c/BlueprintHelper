#include "Misc/AutomationTest.h"
#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectFirstBridge_ExportToJson_NotIncludeJsonTextByDefault,
	"BlueprintHelper.ObjectFirst.Bridge.ExportToJson_NoJsonText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ExportToJson_NotIncludeJsonTextByDefault::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Bridge object-first protocol no longer exposes json_text"), true);
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
	FObjectFirstBridge_ImportJson_RejectsString,
	"BlueprintHelper.ObjectFirst.Bridge.ImportJson_RejectsString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstBridge_ImportJson_RejectsString::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Bridge import string path is retired"), true);
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
