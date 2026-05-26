#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCallableCatalogContractTest,
	"BlueprintHelper.GraphWrite.OpCoverage.Evidence.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCallableCatalogContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("new callable op count"), FBlueprintHelperOpCallableCatalog::GetSupportedCallableSpecs().Num(), 38);
	TestEqual(TEXT("type promotion op count"), FBlueprintHelperOpCallableCatalog::GetTypePromotionOperationIds().Num(), 10);
	TestTrue(TEXT("enum_equal excluded"), FBlueprintHelperOpCallableCatalog::FindExcludedSpec(TEXT("enum_equal")) != nullptr);
	TestTrue(TEXT("enum_not_equal excluded"), FBlueprintHelperOpCallableCatalog::FindExcludedSpec(TEXT("enum_not_equal")) != nullptr);

	const FBlueprintHelperOpCallableSpec* ArrayIdentical = FBlueprintHelperOpCallableCatalog::FindSupportedSpec(TEXT("array_identical"));
	TestNotNull(TEXT("array_identical spec"), ArrayIdentical);
	if (ArrayIdentical)
	{
		TestTrue(TEXT("array_identical requires lhs evidence"), ArrayIdentical->RequiredEvidenceKeys.Contains(TEXT("op.array_lhs_pin_type")));
		TestTrue(TEXT("array_identical requires rhs evidence"), ArrayIdentical->RequiredEvidenceKeys.Contains(TEXT("op.array_rhs_pin_type")));
	}

	const FBlueprintHelperOpCallableSpec* BooleanAnd = FBlueprintHelperOpCallableCatalog::FindSupportedSpec(TEXT("boolean_and"));
	TestNotNull(TEXT("boolean_and spec"), BooleanAnd);
	if (BooleanAnd)
	{
		TestEqual(TEXT("boolean_and spawn family"), BooleanAnd->SpawnFamily, FString(TEXT("commutative_function")));
	}

	const FBlueprintHelperOpCallableSpec* Abs = FBlueprintHelperOpCallableCatalog::FindSupportedSpec(TEXT("abs"));
	TestNotNull(TEXT("abs spec"), Abs);
	if (Abs)
	{
		TestEqual(TEXT("abs spawn family"), Abs->SpawnFamily, FString(TEXT("call_function_compact")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOpCallableEvidenceReaderTest,
	"BlueprintHelper.GraphWrite.OpCoverage.Evidence.Reader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOpCallableEvidenceReaderTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.FunctionOperation = TEXT("op.boolean_and");

	FBlueprintHelperOpCallableEvidence Evidence;
	FString ErrorCode;
	FString Message;
	TestTrue(
		TEXT("boolean_and evidence reads from semantic function operation"),
		FBlueprintHelperOpCallableEvidenceReader::Read(Request, Evidence, ErrorCode, Message));
	TestEqual(TEXT("boolean_and operation id"), Evidence.OperationId, FString(TEXT("boolean_and")));
	TestEqual(TEXT("boolean_and spawn family"), Evidence.Spec.SpawnFamily, FString(TEXT("commutative_function")));

	FBlueprintHelperActionResolutionRequest MissingEvidenceRequest;
	MissingEvidenceRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	MissingEvidenceRequest.ContextEvidence.Add(TEXT("op.operation_id"), TEXT("array_identical"));
	TestFalse(
		TEXT("array_identical missing typed evidence rejected"),
		FBlueprintHelperOpCallableEvidenceReader::Read(MissingEvidenceRequest, Evidence, ErrorCode, Message));
	TestEqual(TEXT("array_identical missing evidence code"), ErrorCode, FString(TEXT("missing_op_evidence.op.array_lhs_pin_type")));

	FBlueprintHelperActionResolutionRequest ExcludedRequest;
	ExcludedRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	ExcludedRequest.ContextEvidence.Add(TEXT("op.operation_id"), TEXT("enum_equal"));
	TestFalse(
		TEXT("excluded op rejected"),
		FBlueprintHelperOpCallableEvidenceReader::Read(ExcludedRequest, Evidence, ErrorCode, Message));
	TestEqual(TEXT("excluded error code"), ErrorCode, FString(TEXT("excluded_op_operation")));

	FBlueprintHelperActionResolutionRequest UnknownRequest;
	UnknownRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	UnknownRequest.ContextEvidence.Add(TEXT("op.operation_id"), TEXT("not_a_real_op"));
	TestFalse(
		TEXT("unknown op rejected"),
		FBlueprintHelperOpCallableEvidenceReader::Read(UnknownRequest, Evidence, ErrorCode, Message));
	TestEqual(TEXT("unknown error code"), ErrorCode, FString(TEXT("unsupported_op_operation")));

	return true;
}

#endif
