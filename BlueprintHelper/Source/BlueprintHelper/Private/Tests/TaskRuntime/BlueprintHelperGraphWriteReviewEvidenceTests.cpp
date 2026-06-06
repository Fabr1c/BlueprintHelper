#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteReviewEvidenceBuilder.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

static TSharedPtr<FJsonObject> BlueprintHelperGraphWriteReviewEvidenceParseJsonObject(const FString& JsonText)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	FJsonSerializer::Deserialize(Reader, Json);
	return Json;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReviewEvidencePreservesCompileDiagnosticsTest,
	"BlueprintHelper.TaskRuntime.GraphWrite.ReviewEvidence.PreservesCompileDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReviewEvidencePreservesCompileDiagnosticsTest::RunTest(const FString& Parameters)
{
	const FString GraphName = TEXT("EventGraph");
	const FString NodeGuid = TEXT("11111111-2222-3333-4444-555555555555");
	const FString TargetKey = TEXT("graph:EventGraph:block:EventGraph_generated_block");

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_GraphWriteReviewEvidence"));
	Target->SetStringField(TEXT("graph"), GraphName);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = TEXT("graph_write");
	LoweredStep.AdapterOperation = TEXT("append_blueprint_graph");
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetObjectField(TEXT("target"), Target);

	TSharedRef<FJsonObject> DiagnosticJson = MakeShared<FJsonObject>();
	DiagnosticJson->SetStringField(TEXT("severity"), TEXT("error"));
	DiagnosticJson->SetStringField(TEXT("code"), TEXT("blueprint_compile_node_error"));
	DiagnosticJson->SetStringField(TEXT("message"), TEXT("Generated call is invalid."));
	DiagnosticJson->SetStringField(TEXT("graph_name"), GraphName);
	DiagnosticJson->SetStringField(TEXT("node_guid"), NodeGuid);
	DiagnosticJson->SetStringField(TEXT("node_title"), TEXT("Generated Print String"));
	DiagnosticJson->SetStringField(TEXT("node_class"), TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));
	DiagnosticJson->SetStringField(TEXT("error_type"), TEXT("compiler"));
	DiagnosticJson->SetStringField(TEXT("block_ref"), TEXT("generated_block"));
	DiagnosticJson->SetStringField(TEXT("target_key"), TargetKey);

	TArray<TSharedPtr<FJsonValue>> CompilerResults;
	CompilerResults.Add(MakeShared<FJsonValueObject>(DiagnosticJson));

	TSharedRef<FJsonObject> CompileResult = MakeShared<FJsonObject>();
	CompileResult->SetArrayField(TEXT("compiler_results"), CompilerResults);

	TArray<TSharedPtr<FJsonValue>> BlockRefs;
	BlockRefs.Add(MakeShared<FJsonValueString>(TEXT("generated_block")));

	TSharedRef<FJsonObject> GeneratedNodeJson = MakeShared<FJsonObject>();
	GeneratedNodeJson->SetStringField(TEXT("graph_name"), GraphName);
	GeneratedNodeJson->SetStringField(TEXT("node_guid"), NodeGuid);
	GeneratedNodeJson->SetStringField(TEXT("node_title"), TEXT("Generated Print String"));
	GeneratedNodeJson->SetStringField(TEXT("node_class"), TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));
	GeneratedNodeJson->SetStringField(TEXT("compile_diagnostic_correlation_key"), GraphName + TEXT(":") + NodeGuid);
	GeneratedNodeJson->SetStringField(TEXT("target_key"), TargetKey);

	TArray<TSharedPtr<FJsonValue>> GeneratedNodes;
	GeneratedNodes.Add(MakeShared<FJsonValueObject>(GeneratedNodeJson));

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	StepResult.Data->SetArrayField(TEXT("block_refs"), BlockRefs);
	StepResult.Data->SetObjectField(TEXT("compile_result"), CompileResult);
	StepResult.Data->SetArrayField(TEXT("generated_nodes"), GeneratedNodes);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive_compile_diag"),
		TEXT("task_compile_diag"),
		4,
		Evidence);

	TestTrue(TEXT("graphwrite evidence builds"), bBuilt);
	TestEqual(TEXT("one target is emitted"), Evidence.AtomicTargets.Num(), 1);
	TestTrue(TEXT("evidence carries diagnostics"), Evidence.Diagnostics.Num() >= 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& AtomicTarget = Evidence.AtomicTargets[0];
	TestTrue(TEXT("atomic target carries diagnostics"), AtomicTarget.Diagnostics.Num() >= 1);
	if (AtomicTarget.Diagnostics.Num() == 0)
	{
		return false;
	}

	const FBlueprintHelperDiagnosticItem& Diagnostic = AtomicTarget.Diagnostics[0];
	TestEqual(TEXT("diagnostic code preserved"),
		Diagnostic.Code,
		FString(TEXT("blueprint_compile_node_error")));
	TestEqual(TEXT("diagnostic graph preserved"), Diagnostic.GraphName, GraphName);
	TestEqual(TEXT("diagnostic node guid preserved"), Diagnostic.NodeGuid, NodeGuid);
	TestEqual(TEXT("diagnostic node title preserved"),
		Diagnostic.NodeTitle,
		FString(TEXT("Generated Print String")));
	TestEqual(TEXT("diagnostic node class preserved"),
		Diagnostic.NodeClass,
		FString(TEXT("/Script/BlueprintGraph.K2Node_CallFunction")));
	TestEqual(TEXT("diagnostic block ref preserved"),
		Diagnostic.BlockRef,
		FString(TEXT("generated_block")));
	TestEqual(TEXT("diagnostic target key preserved"), Diagnostic.TargetKey, TargetKey);
	TestEqual(TEXT("diagnostic correlation key derived from graph and node"),
		Diagnostic.CompileDiagnosticCorrelationKey,
		GraphName + TEXT(":") + NodeGuid);

	bool bReadbackCorrelationPreserved = false;
	for (const FBlueprintHelperDiagnosticItem& Candidate : Evidence.Diagnostics)
	{
		bReadbackCorrelationPreserved |= Candidate.CompileDiagnosticCorrelationKey == GraphName + TEXT(":") + NodeGuid;
	}
	TestTrue(TEXT("readback correlation key is preserved on evidence"), bReadbackCorrelationPreserved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReviewEvidenceDoesNotFanOutNodeDiagnosticsTest,
	"BlueprintHelper.TaskRuntime.GraphWrite.ReviewEvidence.DoesNotFanOutNodeDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReviewEvidenceDoesNotFanOutNodeDiagnosticsTest::RunTest(const FString& Parameters)
{
	const FString GraphName = TEXT("EventGraph");
	const FString NodeGuid = TEXT("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_GraphWriteReviewEvidenceFanOut"));
	Target->SetStringField(TEXT("graph"), GraphName);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = TEXT("graph_write");
	LoweredStep.AdapterOperation = TEXT("append_blueprint_graph");
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetObjectField(TEXT("target"), Target);

	TSharedRef<FJsonObject> NodeDiagnosticJson = MakeShared<FJsonObject>();
	NodeDiagnosticJson->SetStringField(TEXT("severity"), TEXT("error"));
	NodeDiagnosticJson->SetStringField(TEXT("code"), TEXT("blueprint_compile_node_error"));
	NodeDiagnosticJson->SetStringField(TEXT("message"), TEXT("Generated node is invalid."));
	NodeDiagnosticJson->SetStringField(TEXT("graph_name"), GraphName);
	NodeDiagnosticJson->SetStringField(TEXT("node_guid"), NodeGuid);
	NodeDiagnosticJson->SetStringField(TEXT("node_title"), TEXT("Generated Node Without Target"));

	TArray<TSharedPtr<FJsonValue>> CompilerResults;
	CompilerResults.Add(MakeShared<FJsonValueObject>(NodeDiagnosticJson));

	TSharedRef<FJsonObject> CompileResult = MakeShared<FJsonObject>();
	CompileResult->SetArrayField(TEXT("compiler_results"), CompilerResults);

	TArray<TSharedPtr<FJsonValue>> BlockRefs;
	BlockRefs.Add(MakeShared<FJsonValueString>(TEXT("first_block")));
	BlockRefs.Add(MakeShared<FJsonValueString>(TEXT("second_block")));

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	StepResult.Data->SetArrayField(TEXT("block_refs"), BlockRefs);
	StepResult.Data->SetObjectField(TEXT("compile_result"), CompileResult);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive_compile_diag_fanout"),
		TEXT("task_compile_diag_fanout"),
		5,
		Evidence);

	TestTrue(TEXT("graphwrite evidence builds"), bBuilt);
	TestEqual(TEXT("two targets are emitted"), Evidence.AtomicTargets.Num(), 2);
	TestEqual(TEXT("evidence-level node diagnostic is preserved"), Evidence.Diagnostics.Num(), 1);
	for (const FBlueprintHelperReviewAtomicTarget& AtomicTarget : Evidence.AtomicTargets)
	{
		TestEqual(TEXT("node diagnostic without target identity is not copied to graph-block target"),
			AtomicTarget.Diagnostics.Num(),
			0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReviewEvidenceCarriesGraphBodyBoundaryTest,
	"BlueprintHelper.TaskRuntime.GraphWriteReviewEvidence.CarriesGraphBodyBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReviewEvidenceCarriesGraphBodyBoundaryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = TEXT("k2.macro_body");
	Boundary.TaskSpecStrategy = TEXT("replace_owned_graph");
	Boundary.TargetAssetPath = TEXT("/Game/BP_GraphWriteReviewEvidence");
	Boundary.GraphName = TEXT("ClampScoreMacro");
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2MacroBody;
	Boundary.EntryNodeRefs.Add(TEXT("TunnelEntry"));
	Boundary.ExitNodeRefs.Add(TEXT("TunnelExit"));

	TSharedRef<FJsonObject> Evidence =
		FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildGraphBodyBoundaryEvidence(Boundary);
	TestEqual(TEXT("adapter id"),
		Evidence->GetStringField(TEXT("runtime_adapter_id")),
		FString(TEXT("k2.macro_body")));
	TestEqual(TEXT("body kind"),
		Evidence->GetStringField(TEXT("body_kind")),
		FString(TEXT("k2.macro_body")));

	const TArray<TSharedPtr<FJsonValue>>* EntryBoundaries = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ExitBoundaries = nullptr;
	TestTrue(TEXT("entry boundary exists"),
		Evidence->TryGetArrayField(TEXT("entry_boundaries"), EntryBoundaries) &&
		EntryBoundaries &&
		EntryBoundaries->Num() == 1);
	TestTrue(TEXT("exit boundary exists"),
		Evidence->TryGetArrayField(TEXT("exit_boundaries"), ExitBoundaries) &&
		ExitBoundaries &&
		ExitBoundaries->Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReviewEvidenceBuildUsesRuntimeBoundaryTest,
	"BlueprintHelper.TaskRuntime.GraphWriteReviewEvidence.BuildUsesRuntimeBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReviewEvidenceBuildUsesRuntimeBoundaryTest::RunTest(const FString& Parameters)
{
	const FString GraphName = TEXT("ClampScoreMacro");

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_GraphWriteReviewEvidence"));
	Target->SetStringField(TEXT("graph"), GraphName);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = TEXT("graph_write");
	LoweredStep.AdapterOperation = TEXT("replace_blueprint_graph");
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetObjectField(TEXT("target"), Target);

	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = TEXT("k2.macro_body");
	Boundary.TaskSpecStrategy = TEXT("replace_owned_graph");
	Boundary.TargetAssetPath = TEXT("/Game/BP_GraphWriteReviewEvidence");
	Boundary.GraphName = GraphName;
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2MacroBody;
	Boundary.EntryNodeRefs.Add(TEXT("TunnelEntry"));
	Boundary.ExitNodeRefs.Add(TEXT("TunnelExit"));

	TArray<TSharedPtr<FJsonValue>> BlockRefs;
	BlockRefs.Add(MakeShared<FJsonValueString>(TEXT("ClampScoreMacro")));

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	StepResult.Data->SetArrayField(TEXT("block_refs"), BlockRefs);
	StepResult.Data->SetObjectField(
		TEXT("graph_body_boundary"),
		FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildGraphBodyBoundaryEvidence(Boundary));

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive_graph_body_boundary"),
		TEXT("task_graph_body_boundary"),
		6,
		Evidence);

	TestTrue(TEXT("graphwrite evidence builds"), bBuilt);
	TestEqual(TEXT("one target is emitted"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& AtomicTarget = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target subkind is runtime body kind"),
		AtomicTarget.TargetSubKind,
		FString(TEXT("k2.macro_body")));
	TestFalse(TEXT("graph body boundary json exists"), AtomicTarget.GraphBodyBoundaryJson.IsEmpty());

	const TSharedPtr<FJsonObject> BoundaryJson =
		BlueprintHelperGraphWriteReviewEvidenceParseJsonObject(AtomicTarget.GraphBodyBoundaryJson);
	TestTrue(TEXT("boundary json parses"), BoundaryJson.IsValid());
	if (!BoundaryJson.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("boundary adapter id"),
		BoundaryJson->GetStringField(TEXT("runtime_adapter_id")),
		FString(TEXT("k2.macro_body")));
	TestTrue(TEXT("boundary entry array exists"), BoundaryJson->HasTypedField<EJson::Array>(TEXT("entry_boundaries")));
	TestTrue(TEXT("boundary exit array exists"), BoundaryJson->HasTypedField<EJson::Array>(TEXT("exit_boundaries")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteReviewEvidenceConnectivityFailureCleanupContractTest,
	"BlueprintHelper.TaskRuntime.GraphWriteReviewEvidence.ConnectivityFailureCleanupContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteReviewEvidenceConnectivityFailureCleanupContractTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp"));
	FString Source;
	TestTrue(TEXT("replace service source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));

	const int32 FailureBranchIndex = Source.Find(TEXT("if (!GenerateResult.bSucceed && !bDeferredEntryResolvedConnectivityFailure)"));
	const int32 RestoreIndex = Source.Find(TEXT("RestoreReplacementFailureSnapshot"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FailureBranchIndex);
	const int32 FailureResultIndex = Source.Find(TEXT("ToolResultBuilder::Failure"), ESearchCase::CaseSensitive, ESearchDir::FromStart, RestoreIndex);
	const int32 BoundaryEvidenceIndex = Source.Find(TEXT("graph_body_boundary"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FailureResultIndex);
	TestTrue(TEXT("connectivity failure branch exists"), FailureBranchIndex != INDEX_NONE);
	TestTrue(TEXT("restore happens in connectivity failure branch"), RestoreIndex != INDEX_NONE);
	TestTrue(TEXT("failure result is built after restore"), FailureResultIndex != INDEX_NONE && RestoreIndex < FailureResultIndex);
	TestTrue(TEXT("failure result keeps adapter boundary evidence"), BoundaryEvidenceIndex != INDEX_NONE && FailureResultIndex < BoundaryEvidenceIndex);
	TestTrue(TEXT("rollback result reports rolled back"),
		Source.Contains(TEXT("Error.RollbackResult = bRestored")) &&
		Source.Contains(TEXT("EBlueprintHelperRollbackResult::RolledBack")));
	return true;
}

#endif
