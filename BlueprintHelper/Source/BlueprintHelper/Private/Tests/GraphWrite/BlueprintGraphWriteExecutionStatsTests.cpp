#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityDiagnosticsJson.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteExecutionStatsToJsonTest,
	"BlueprintHelper.GraphWrite.ExecutionStats.ToJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteExecutionStatsToJsonTest::RunTest(const FString&)
{
	FBlueprintGraphWriteExecutionStats Stats;
	Stats.RequestedNodeCount = 3;
	Stats.SpawnedNodeCount = 3;
	Stats.RequestedDefaultValueCount = 2;
	Stats.AppliedDefaultValueCount = 2;
	Stats.RequestedLinkCount = 2;
	Stats.CreatedLinkCount = 2;
	Stats.LayoutRecordNodeCount = 3;
	Stats.SpawnNodesMs = 11.5;
	Stats.ApplyDefaultsMs = 2.25;
	Stats.ConnectLinksMs = 3.75;
	Stats.RecordLayoutMs = 1.0;
	Stats.ConnectivityViolationCount = 4;
	Stats.ConnectivityValidationMs = 0.5;

	const TSharedRef<FJsonObject> Json = FBlueprintGraphWriteExecutionStatsSerializer::ToJson(Stats);
	TestEqual(TEXT("spawned node count"), Json->GetIntegerField(TEXT("spawned_node_count")), 3);
	TestEqual(TEXT("created link count"), Json->GetIntegerField(TEXT("created_link_count")), 2);
	TestEqual(TEXT("spawn ms"), Json->GetNumberField(TEXT("spawn_nodes_ms")), 11.5);
	TestEqual(TEXT("connectivity violation count"), Json->GetIntegerField(TEXT("connectivity_violation_count")), 4);
	TestEqual(TEXT("connectivity validation ms"), Json->GetNumberField(TEXT("connectivity_validation_ms")), 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGenerateResultTracksConnectivityDiagnosticsTest,
	"BlueprintHelper.GraphWrite.GenerateResult.TracksConnectivityDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGenerateResultTracksConnectivityDiagnosticsTest::RunTest(const FString&)
{
	FBlueprintGenerateResult Result;
	FBlueprintGeneratorDiagnostic Diagnostic;
	Diagnostic.Code = TEXT("unconsumed_pure_data_node");
	Diagnostic.NodeId = TEXT("node_a");
	Diagnostic.Message = TEXT("Generated PureData node has no outgoing data consumer.");

	Result.ConnectivityDiagnostics.Add(Diagnostic);
	Result.ConnectivityViolationCount = Result.ConnectivityDiagnostics.Num();

	TestEqual(TEXT("connectivity diagnostics count"), Result.ConnectivityDiagnostics.Num(), 1);
	TestEqual(TEXT("connectivity violation count"), Result.ConnectivityViolationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintGraphWriteConnectivityDiagnosticsJsonAttachesConciseViolationsTest,
	"BlueprintHelper.GraphWrite.ConnectivityDiagnosticsJson.AttachesConciseViolations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintGraphWriteConnectivityDiagnosticsJsonAttachesConciseViolationsTest::RunTest(const FString&)
{
	FBlueprintGeneratorDiagnostic Diagnostic;
	Diagnostic.Code = TEXT("unconsumed_pure_data_node");
	Diagnostic.NodeId = TEXT("node_a");
	Diagnostic.PinName = TEXT("value");
	Diagnostic.Message = TEXT("Generated PureData node has no outgoing data consumer.");

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	FBlueprintHelperGraphWriteConnectivityDiagnosticsJson::Attach(Data, { Diagnostic });

	const TSharedPtr<FJsonObject>* Connectivity = nullptr;
	TestTrue(
		TEXT("connectivity object exists"),
		Data->TryGetObjectField(TEXT("connectivity"), Connectivity) && Connectivity && Connectivity->IsValid());
	if (!Connectivity || !Connectivity->IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Violations = nullptr;
	TestTrue(
		TEXT("violations array exists"),
		(*Connectivity)->TryGetArrayField(TEXT("violations"), Violations) && Violations);
	if (!Violations || Violations->Num() != 1 || !(*Violations)[0].IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Violation = (*Violations)[0]->AsObject();
	TestTrue(TEXT("violation object valid"), Violation.IsValid());
	if (!Violation.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("code"), Violation->GetStringField(TEXT("code")), FString(TEXT("unconsumed_pure_data_node")));
	TestEqual(TEXT("node_id"), Violation->GetStringField(TEXT("node_id")), FString(TEXT("node_a")));
	TestEqual(TEXT("message"), Violation->GetStringField(TEXT("message")), Diagnostic.Message);
	TestFalse(TEXT("pin_name omitted from ordinary output"), Violation->HasField(TEXT("pin_name")));
	return true;
}

#endif
