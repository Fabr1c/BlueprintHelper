#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeOpCoverageObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeOpCoverageBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperOpCoverage/%s"),
		*MakeOpCoverageObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeOpCoverageObjectName(TEXT("BP_OpCoverage")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperOpCoverageEndToEndTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static FBlueprintHelperActionResolutionRequest MakeOpCoverageRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& OperationId)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeOpCoverageObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("op_coverage_projected_context");
	Request.SemanticConstraintsHash = TEXT("op_coverage_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Operator;
	Request.Semantic.FunctionOperation = FString::Printf(TEXT("op.%s"), *OperationId);
	Request.ContextEvidence.Add(TEXT("op.operation_id"), OperationId);
	return Request;
}

static bool BuildOpCoverageGraph(UBlueprint*& OutBlueprint, UEdGraph*& OutGraph)
{
	OutBlueprint = MakeOpCoverageBlueprint();
	OutGraph = OutBlueprint && OutBlueprint->UbergraphPages.Num() > 0 ? OutBlueprint->UbergraphPages[0] : nullptr;
	return OutBlueprint && OutGraph;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCoverageE2E_TypePromotionReadbackFactsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.E2E.TypePromotionReadbackFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCoverageE2E_TypePromotionReadbackFactsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	TestTrue(TEXT("graph built"), BuildOpCoverageGraph(Blueprint, Graph));
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(MakeOpCoverageRequest(Blueprint, Graph, TEXT("add")));

	FString Failure;
	const bool bVerified = FBlueprintHelperOpCoverageReadbackVerifier::Verify(
		Result,
		FBlueprintHelperOpCoverageReadbackExpectation{
			TEXT("add"),
			TEXT("promotable_operator:Add"),
			TEXT("/Script/BlueprintGraph.K2Node_PromotableOperator"),
			FString(),
			{},
			true
		},
		Failure);
	if (!bVerified)
	{
		AddError(Failure);
	}
	return bVerified;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCoverageE2E_BooleanAndReadbackFactsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.E2E.BooleanAndReadbackFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCoverageE2E_BooleanAndReadbackFactsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	TestTrue(TEXT("graph built"), BuildOpCoverageGraph(Blueprint, Graph));
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(MakeOpCoverageRequest(Blueprint, Graph, TEXT("boolean_and")));

	FString Failure;
	const bool bVerified = FBlueprintHelperOpCoverageReadbackVerifier::Verify(
		Result,
		FBlueprintHelperOpCoverageReadbackExpectation{
			TEXT("boolean_and"),
			TEXT("/Script/Engine.KismetMathLibrary:BooleanAND"),
			TEXT("/Script/BlueprintGraph.K2Node_CommutativeAssociativeBinaryOperator"),
			TEXT("bool"),
			{ TEXT("A"), TEXT("B") },
			false
		},
		Failure);
	if (!bVerified)
	{
		AddError(Failure);
	}
	return bVerified;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCoverageE2E_ArrayIdenticalReadbackFactsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.E2E.ArrayIdenticalReadbackFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCoverageE2E_ArrayIdenticalReadbackFactsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	TestTrue(TEXT("graph built"), BuildOpCoverageGraph(Blueprint, Graph));
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request = MakeOpCoverageRequest(Blueprint, Graph, TEXT("array_identical"));
	Request.ContextEvidence.Add(TEXT("op.array_lhs_pin_type"), TEXT("array|int"));
	Request.ContextEvidence.Add(TEXT("op.array_rhs_pin_type"), TEXT("array|int"));
	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);

	FString Failure;
	const bool bVerified = FBlueprintHelperOpCoverageReadbackVerifier::Verify(
		Result,
		FBlueprintHelperOpCoverageReadbackExpectation{
			TEXT("array_identical"),
			TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical"),
			TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction"),
			TEXT("bool"),
			{ TEXT("ArrayA"), TEXT("ArrayB") },
			false
		},
		Failure);
	if (!bVerified)
	{
		AddError(Failure);
	}
	return bVerified;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCoverageE2E_DeterministicNegativeDiagnosticsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.E2E.DeterministicNegativeDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCoverageE2E_DeterministicNegativeDiagnosticsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	TestTrue(TEXT("graph built"), BuildOpCoverageGraph(Blueprint, Graph));
	if (!Blueprint || !Graph)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("unsupported op code"),
		FBlueprintHelperActionResolutionCore::Resolve(MakeOpCoverageRequest(Blueprint, Graph, TEXT("not_real_op"))).ErrorCode,
		FString(TEXT("unsupported_op_operation")));
	bPassed &= TestEqual(
		TEXT("excluded op code"),
		FBlueprintHelperActionResolutionCore::Resolve(MakeOpCoverageRequest(Blueprint, Graph, TEXT("enum_equal"))).ErrorCode,
		FString(TEXT("excluded_op_operation")));
	bPassed &= TestEqual(
		TEXT("missing array evidence code"),
		FBlueprintHelperActionResolutionCore::Resolve(MakeOpCoverageRequest(Blueprint, Graph, TEXT("array_identical"))).ErrorCode,
		FString(TEXT("array_typed_pin_missing")));

	FBlueprintHelperActionResolutionRequest Mismatch = MakeOpCoverageRequest(Blueprint, Graph, TEXT("array_identical"));
	Mismatch.ContextEvidence.Add(TEXT("op.array_lhs_pin_type"), TEXT("array|int"));
	Mismatch.ContextEvidence.Add(TEXT("op.array_rhs_pin_type"), TEXT("array|bool"));
	bPassed &= TestEqual(
		TEXT("mismatched array evidence code"),
		FBlueprintHelperActionResolutionCore::Resolve(Mismatch).ErrorCode,
		FString(TEXT("array_typed_pin_mismatch")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCoverageReadbackRejectsMismatchedFirstCandidateTest,
	"BlueprintHelper.GraphWrite.OpCoverage.Readback.RejectsMismatchedFirstCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCoverageReadbackRejectsMismatchedFirstCandidateTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.SelectedStableId = TEXT("promotable_operator:Add");

	FBlueprintHelperCallFunctionCandidateInfo WrongCandidate;
	WrongCandidate.StableId = TEXT("promotable_operator:Less");
	WrongCandidate.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_PromotableOperator");
	WrongCandidate.ReadbackFacts.Add(TEXT("op.operation_id"), TEXT("less"));
	WrongCandidate.ReadbackFacts.Add(TEXT("op.node_class_path"), WrongCandidate.NodeClassPath);
	WrongCandidate.ReadbackFacts.Add(TEXT("op.wildcard_residual"), TEXT("false"));
	WrongCandidate.ReadbackFacts.Add(TEXT("op.spawner_class"), TEXT("UBlueprintFunctionNodeSpawner"));
	WrongCandidate.ReadbackFacts.Add(TEXT("op.type_promotion_operator"), TEXT("Less"));

	FBlueprintHelperCallFunctionCandidateInfo SelectedCandidate;
	SelectedCandidate.StableId = Result.SelectedStableId;
	SelectedCandidate.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_PromotableOperator");
	SelectedCandidate.ReadbackFacts.Add(TEXT("op.operation_id"), TEXT("add"));
	SelectedCandidate.ReadbackFacts.Add(TEXT("op.node_class_path"), SelectedCandidate.NodeClassPath);
	SelectedCandidate.ReadbackFacts.Add(TEXT("op.wildcard_residual"), TEXT("false"));
	SelectedCandidate.ReadbackFacts.Add(TEXT("op.spawner_class"), TEXT("UBlueprintFunctionNodeSpawner"));
	SelectedCandidate.ReadbackFacts.Add(TEXT("op.type_promotion_operator"), TEXT("Add"));

	Result.CandidateActions.Add(WrongCandidate);
	Result.CandidateActions.Add(SelectedCandidate);
	Result.FunctionCandidate.StableId = Result.SelectedStableId;
	Result.FunctionCandidate.NodeClassPath = SelectedCandidate.NodeClassPath;

	FString Failure;
	const bool bVerified = FBlueprintHelperOpCoverageReadbackVerifier::Verify(
		Result,
		FBlueprintHelperOpCoverageReadbackExpectation{
			TEXT("add"),
			Result.SelectedStableId,
			TEXT("/Script/BlueprintGraph.K2Node_PromotableOperator"),
			FString(),
			{},
			true
		},
		Failure);
	TestTrue(TEXT("readback verifier uses selected candidate evidence"), bVerified);
	if (!bVerified)
	{
		AddError(Failure);
	}
	return bVerified;
}

#endif
