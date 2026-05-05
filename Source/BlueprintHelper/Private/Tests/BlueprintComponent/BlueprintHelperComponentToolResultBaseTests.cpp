#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Services/BlueprintComponent/BlueprintHelperComponentService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"

namespace
{
	void AssertComponentToolResultBaseEnvelope(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedOperation)
	{
		Test.TestEqual(TEXT("schema is ToolResultBase schema"),
			Result.Schema, FString(BlueprintHelperProtocol::ToolResultSchema));
		Test.TestEqual(TEXT("operation is preserved"), Result.Operation, ExpectedOperation);
		Test.TestEqual(TEXT("failed status is represented by ToolResultBase"),
			Result.Status, EBlueprintHelperToolStatus::Failed);
		Test.TestFalse(TEXT("failed component result is not modified"), Result.bModified);
		Test.TestTrue(TEXT("error is carried by ToolResultBase"), Result.Error.IsSet());
		Test.TestNotNull(TEXT("component data is still present under data"), Result.Data.Get());

		const TSharedRef<FJsonObject> Json = Result.ToJson();
		FString Schema;
		FString Operation;
		FString Status;
		Test.TestTrue(TEXT("json carries schema"), Json->TryGetStringField(TEXT("schema"), Schema));
		Test.TestTrue(TEXT("json carries operation"), Json->TryGetStringField(TEXT("operation"), Operation));
		Test.TestTrue(TEXT("json carries status"), Json->TryGetStringField(TEXT("status"), Status));
		Test.TestEqual(TEXT("json schema value"), Schema, FString(BlueprintHelperProtocol::ToolResultSchema));
		Test.TestEqual(TEXT("json operation value"), Operation, ExpectedOperation);
		Test.TestEqual(TEXT("json status value"), Status, FString(TEXT("failed")));
		Test.TestTrue(TEXT("json carries target"), Json->HasTypedField<EJson::Object>(TEXT("target")));
		Test.TestTrue(TEXT("json carries data"), Json->HasTypedField<EJson::Object>(TEXT("data")));
		Test.TestTrue(TEXT("json carries error"), Json->HasTypedField<EJson::Object>(TEXT("error")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentToolResultBaseReadEnvelopeTest,
	"BlueprintHelper.Component.ToolResultBase.ReadEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentToolResultBaseReadEnvelopeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperReadComponentsRequest Request;
	Request.AssetPath = TEXT("/Game/BlueprintHelper/DoesNotExist/BP_Missing");

	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(Request);
	AssertComponentToolResultBaseEnvelope(*this, Result, TEXT("read_components"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentToolResultBaseWriteEnvelopeTest,
	"BlueprintHelper.Component.ToolResultBase.WriteEnvelopes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentToolResultBaseWriteEnvelopeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	const FString MissingAssetPath = TEXT("/Game/BlueprintHelper/DoesNotExist/BP_Missing");

	FBlueprintHelperAddComponentRequest AddRequest;
	AddRequest.AssetPath = MissingAssetPath;
	AddRequest.ComponentName = TEXT("TestComponent");
	AddRequest.ComponentClass = TEXT("StaticMeshComponent");
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.AddComponent(AddRequest),
		TEXT("add_component"));

	FBlueprintHelperSetComponentPropertiesRequest SetRequest;
	SetRequest.AssetPath = MissingAssetPath;
	SetRequest.ComponentName = TEXT("TestComponent");
	SetRequest.Mode = EBlueprintHelperComponentPropertyMode::Single;
	FBlueprintHelperComponentPropertySetting Setting;
	Setting.PropertyPath = TEXT("Mobility");
	Setting.Value = MakeShared<FJsonValueString>(TEXT("Movable"));
	SetRequest.Settings.Add(MoveTemp(Setting));
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.SetComponentProperty(SetRequest),
		TEXT("set_component_property"));

	FBlueprintHelperRemoveComponentRequest RemoveRequest;
	RemoveRequest.AssetPath = MissingAssetPath;
	RemoveRequest.ComponentName = TEXT("TestComponent");
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.RemoveComponent(RemoveRequest),
		TEXT("remove_component"));

	return true;
}

#endif
