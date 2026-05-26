#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericOpsDebugObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericOpsDebugBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericOpsDebug/%s"),
		*MakeGenericOpsDebugObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericOpsDebugObjectName(TEXT("BP_GenericOpsDebug")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericOpsDebugTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericOpsDebugGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeGenericOpsControlRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& Operation)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericOpsDebugObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_ops_debug_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_ops_debug_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control;
	Request.Semantic.Query = Operation;
	Request.Semantic.SearchMode = TEXT("exact");
	Request.Semantic.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	Request.ContextEvidence.Add(TEXT("generic.control.operation"), Operation);
	Request.MaxCandidates = 4;
	return Request;
}

static FBlueprintHelperActionResolutionResult MakeResolvedGenericOpsResult()
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = TEXT("generic_control:switch_enum");
	Result.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_SwitchEnum");

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Result.SelectedStableId;
	Candidate.NodeClassPath = Result.NodeClass;
	Candidate.ReadbackFacts.Add(TEXT("generic.family"), TEXT("control"));
	Candidate.ReadbackFacts.Add(TEXT("generic.operation_id"), TEXT("switch_enum"));
	Candidate.ReadbackFacts.Add(TEXT("generic.control.case_pins"), TEXT("Idle,Running"));
	Candidate.ReadbackFacts.Add(TEXT("generic.wildcard_residual"), TEXT("false"));
	Result.CandidateActions.Add(Candidate);
	return Result;
}

static bool JsonArrayContainsString(
	const TSharedPtr<FJsonObject>& Json,
	const FString& FieldName,
	const FString& Expected)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Values) || !Values)
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (Value.IsValid() && Value->AsString().Equals(Expected, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsActualNodeReadbackTest,
	"BlueprintHelper.GraphWrite.GenericOps.DebugBundle.ActualNodeReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsActualNodeReadbackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericOpsDebugBlueprint();
	UEdGraph* Graph = GetGenericOpsDebugGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericOpsControlRequest(Blueprint, Graph, TEXT("switch_int"));
	Request.ContextEvidence.Add(TEXT("generic.control.case_values"), TEXT("0,1"));
	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("switch_int resolves"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("switch_int no dedicated-builder blocker"), Result.ErrorCode != TEXT("dedicated_fragment_builder_required"));

	FString SpawnError;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Graph,
		Result,
		FVector2D(64.0f, 96.0f),
		SpawnError);
	TestNotNull(TEXT("switch_int spawned node"), SpawnedNode);
	if (!SpawnedNode)
	{
		AddError(SpawnError);
		return false;
	}

	FBlueprintHelperGenericOpsReadbackExpectation Expectation;
	Expectation.Family = TEXT("control");
	Expectation.OperationId = TEXT("generic_ops.control.switch_int");
	Expectation.StableId = Result.SelectedStableId;
	Expectation.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_SwitchInteger");
	Expectation.RequiredFacts.Add(TEXT("generic.control.case_values"), TEXT("0,1"));
	Expectation.RequiredInputPins.Add(TEXT("Selection"));
	Expectation.RequiredOutputPins.Add(TEXT("Default"));

	FString FailureCode;
	FString Failure;
	TestTrue(
		TEXT("generic readback verifies actual node pins"),
		FBlueprintHelperGenericOpsReadbackVerifier::Verify(Result, SpawnedNode, Expectation, FailureCode, Failure));
	if (!FailureCode.IsEmpty() || !Failure.IsEmpty())
	{
		AddError(FString::Printf(TEXT("%s: %s"), *FailureCode, *Failure));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsReadbackVerifierFactsTest,
	"BlueprintHelper.GraphWrite.GenericOps.DebugBundle.ReadbackVerifierFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsReadbackVerifierFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionResult Result = MakeResolvedGenericOpsResult();
	FBlueprintHelperGenericOpsReadbackExpectation Expectation;
	Expectation.Family = TEXT("control");
	Expectation.OperationId = TEXT("switch_enum");
	Expectation.StableId = Result.SelectedStableId;
	Expectation.NodeClassPath = Result.NodeClass;
	Expectation.bRequireSelectedSpawner = false;
	Expectation.RequiredFacts.Add(TEXT("generic.control.case_pins"), TEXT("Idle,Running"));

	FString FailureCode;
	FString Failure;
	TestTrue(
		TEXT("generic readback accepts resolved facts"),
		FBlueprintHelperGenericOpsReadbackVerifier::Verify(Result, Expectation, FailureCode, Failure));
	TestTrue(TEXT("success has no failure code"), FailureCode.IsEmpty());

	Result.CandidateActions[0].ReadbackFacts.Add(TEXT("generic.wildcard_residual"), TEXT("true"));
	TestFalse(
		TEXT("generic readback rejects wildcard residual"),
		FBlueprintHelperGenericOpsReadbackVerifier::Verify(Result, Expectation, FailureCode, Failure));
	TestEqual(TEXT("wildcard residual failure code"), FailureCode, FString(TEXT("wildcard_residual")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsDebugBundleFailureCodesTest,
	"BlueprintHelper.GraphWrite.GenericOps.DebugBundle.FailureCodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsDebugBundleFailureCodesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteCapabilityCaseResult Result;
	Result.CaseName = TEXT("generic_ops_negative_cases");
	Result.Phase = TEXT("GraphWrite");
	Result.Capability = TEXT("GenericOps");
	Result.ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence;
	Result.SemanticKind = TEXT("control|macro|container|transform|create|schedule|struct|select");
	Result.ClusterKind = TEXT("GenericAssetStructControlActionCluster");
	Result.ResolverStatus = TEXT("invalid_request");
	Result.MissingEvidenceFields.Add(TEXT("missing_evidence"));
	Result.MissingEvidenceFields.Add(TEXT("schema_rejection"));
	Result.MissingEvidenceFields.Add(TEXT("wrong_runtime_owner"));
	Result.MissingEvidenceFields.Add(TEXT("macro_pin_shape_snapshot_missing"));
	Result.MissingEvidenceFields.Add(TEXT("macro_spawner_unavailable"));
	Result.MissingEvidenceFields.Add(TEXT("asset_reference_mismatch"));
	Result.MissingEvidenceFields.Add(TEXT("latent_not_allowed"));
	Result.MissingEvidenceFields.Add(TEXT("handler_missing"));
	Result.MissingEvidenceFields.Add(TEXT("wildcard_residual"));
	Result.MissingEvidenceFields.Add(TEXT("expose_on_spawn_pin_missing"));
	Result.MissingEvidenceFields.Add(TEXT("select_result_type_unresolved"));

	const TSharedRef<FJsonObject> Json = FBlueprintHelperGraphWriteCapabilityMetrics::ToDebugBundleFailureSummary(Result);
	bool bPassed = true;
	for (const FString& Code : Result.MissingEvidenceFields)
	{
		bPassed &= TestTrue(
			FString::Printf(TEXT("debug bundle includes GenericOps code %s"), *Code),
			JsonArrayContainsString(Json, TEXT("missing_evidence_fields"), Code));
		bPassed &= TestTrue(
			FString::Printf(TEXT("debug bundle publishes common GenericOps code %s"), *Code),
			JsonArrayContainsString(Json, TEXT("generic_ops_common_error_codes"), Code));
	}
	return bPassed;
}

#endif
