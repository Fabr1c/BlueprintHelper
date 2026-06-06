#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/Debug/BlueprintHelperDiagnosticProjection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDiagnosticProjectionCompileDiagnosticTest,
	"BlueprintHelper.Debug.DiagnosticProjection.CompileDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDiagnosticProjectionCompileDiagnosticTest::RunTest(const FString&)
{
	FBlueprintHelperDiagnosticItem Item;
	Item.Severity = EBlueprintHelperDiagnosticSeverity::Error;
	Item.Code = TEXT("compile_error");
	Item.Message = TEXT("Compile failed.");
	Item.GraphName = TEXT("EventGraph");
	Item.NodeGuid = TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	Item.NodeName = TEXT("PrintString");
	Item.NodeTitle = TEXT("Print String");
	Item.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Item.TargetKey = TEXT("graph:node:PrintString");
	Item.CompileDiagnosticCorrelationKey = TEXT("EventGraph:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	const FBlueprintHelperDiagnosticProjection Projection =
		FBlueprintHelperDiagnosticProjectionUtils::FromDiagnosticItem(
			Item,
			TEXT("compile"),
			TEXT("/Game/BP_Door.BP_Door"),
			TEXT("scope:graph"));

	TestEqual(TEXT("source is preserved"), Projection.Source, FString(TEXT("compile")));
	TestEqual(TEXT("code is preserved"), Projection.Code, Item.Code);
	TestEqual(TEXT("message is preserved"), Projection.Message, Item.Message);
	TestEqual(TEXT("severity is normalized"), Projection.Severity, FString(TEXT("error")));
	TestEqual(TEXT("asset path is preserved"), Projection.AssetPath, FString(TEXT("/Game/BP_Door.BP_Door")));
	TestEqual(TEXT("graph name is preserved"), Projection.GraphName, Item.GraphName);
	TestEqual(TEXT("target key is preserved"), Projection.TargetKey, Item.TargetKey);
	TestEqual(TEXT("scope identity is preserved"), Projection.ScopeIdentity, FString(TEXT("scope:graph")));
	TestTrue(TEXT("details are present"), Projection.Details.IsValid());
	TestEqual(TEXT("node guid is not dropped"), Projection.Details->GetStringField(TEXT("node_guid")), Item.NodeGuid);
	TestEqual(TEXT("correlation key is not dropped"),
		Projection.Details->GetStringField(TEXT("compile_diagnostic_correlation_key")),
		Item.CompileDiagnosticCorrelationKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDiagnosticProjectionGraphWriteDiagnosticTest,
	"BlueprintHelper.Debug.DiagnosticProjection.GraphWriteConnectivityDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDiagnosticProjectionGraphWriteDiagnosticTest::RunTest(const FString&)
{
	FBlueprintHelperDiagnosticItem Item;
	Item.Severity = EBlueprintHelperDiagnosticSeverity::Warning;
	Item.Code = TEXT("unreachable_exec_node");
	Item.Message = TEXT("Generated exec node is unreachable.");
	Item.GraphName = TEXT("EventGraph");
	Item.BlockRef = TEXT("block:DoorLogic");
	Item.PinName = TEXT("execute");
	Item.Field = TEXT("connectivity");

	const FBlueprintHelperDiagnosticProjection Projection =
		FBlueprintHelperDiagnosticProjectionUtils::FromDiagnosticItem(
			Item,
			TEXT("graphwrite.connectivity"),
			TEXT("/Game/BP_Door.BP_Door"),
			TEXT("block:DoorLogic"));

	TestEqual(TEXT("severity is normalized"), Projection.Severity, FString(TEXT("warning")));
	TestEqual(TEXT("graph name is preserved"), Projection.GraphName, Item.GraphName);
	TestEqual(TEXT("scope identity is preserved"), Projection.ScopeIdentity, FString(TEXT("block:DoorLogic")));
	TestEqual(TEXT("block ref is not dropped"), Projection.Details->GetStringField(TEXT("block_ref")), Item.BlockRef);
	TestEqual(TEXT("pin name is not dropped"), Projection.Details->GetStringField(TEXT("pin_name")), Item.PinName);
	TestEqual(TEXT("field is not dropped"), Projection.Details->GetStringField(TEXT("field")), Item.Field);
	return true;
}

#endif
