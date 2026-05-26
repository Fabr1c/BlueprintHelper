#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
static FBlueprintHelperActionResolutionRequest MakeStructSelectEvidenceRequest(const FString& Operation)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.Kind = Operation.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		? EBlueprintHelperActionSemanticKind::Select
		: EBlueprintHelperActionSemanticKind::Construct;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Struct;
	Request.Semantic.Query = Operation;
	Request.Semantic.StructPath = TBaseStructure<FVector>::Get()->GetPathName();
	return Request;
}

static FString MakeStructSelectObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeStructSelectBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperStructSelect/%s"),
		*MakeStructSelectObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeStructSelectObjectName(TEXT("BP_StructSelect")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperStructSelectHardeningTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetStructSelectGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static bool HasDiagnosticCode(const FBlueprintHelperGraphSemanticIR& IR, const FString& Code)
{
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : IR.Diagnostics)
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}

static TSharedRef<FJsonObject> MakeLogicSpecWithStatementKind(const FString& Kind)
{
	TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
	LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));

	TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
	Statement->SetStringField(TEXT("id"), FString::Printf(TEXT("stmt_%s"), *Kind));
	Statement->SetStringField(TEXT("kind"), Kind);

	TArray<TSharedPtr<FJsonValue>> Statements;
	Statements.Add(MakeShared<FJsonValueObject>(Statement));
	LogicSpec->SetArrayField(TEXT("statements"), Statements);
	return LogicSpec;
}

static bool BuildSelectWithResultProof(const FString& ResultTypeProof, FString& OutError)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
	FBlueprintHelperGraphExpressionIR Expression;
	Expression.ExpressionId = TEXT("expr_select_type_proof");
	Expression.Kind = EBlueprintHelperGraphExpressionKind::Select;
	Expression.ContextEvidence.Add(TEXT("generic.select.result_type_proof"), ResultTypeProof);

	FBlueprintHelperActionResolutionResult ActionResult;
	FBlueprintHelperNodeFragment Fragment;
	return FBlueprintHelperSelectFragmentBuilder::Build(Graph, Expression, ActionResult, Fragment, OutError);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsStructSelectEvidenceReaderTest,
	"BlueprintHelper.GraphWrite.GenericOps.StructSelect.EvidenceReader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGenericOpsStructSelectEvidenceReaderTest::RunTest(const FString& Parameters)
{
	FString ErrorCode;
	FString Message;
	FBlueprintHelperGenericOpsStructFieldPolicyEvidence Evidence;

	FBlueprintHelperActionResolutionRequest SetFieldsRequest =
		MakeStructSelectEvidenceRequest(TEXT("set_fields_in_struct"));
	TestFalse(
		TEXT("set_fields_in_struct without selected fields is rejected"),
		FBlueprintHelperStructFieldPolicyEvidenceReader::Read(SetFieldsRequest, Evidence, ErrorCode, Message));
	TestEqual(
		TEXT("set_fields missing field policy error"),
		ErrorCode,
		FString(TEXT("missing_evidence.generic.struct.selected_field_paths")));

	SetFieldsRequest.ContextEvidence.Add(TEXT("generic.struct.selected_field_paths"), TEXT("X,Y,Z"));
	TestTrue(
		TEXT("set_fields_in_struct accepts selected field paths"),
		FBlueprintHelperStructFieldPolicyEvidenceReader::Read(SetFieldsRequest, Evidence, ErrorCode, Message));
	TestEqual(TEXT("selected field count"), Evidence.SelectedFieldPaths.Num(), 3);
	TestTrue(TEXT("struct path retained"), Evidence.StructPath.Contains(TEXT("Vector")));

	FBlueprintHelperActionResolutionRequest SelectRequest =
		MakeStructSelectEvidenceRequest(TEXT("select"));
	SelectRequest.Semantic.StructPath.Reset();
	TestFalse(
		TEXT("select without result type proof is rejected"),
		FBlueprintHelperStructFieldPolicyEvidenceReader::Read(SelectRequest, Evidence, ErrorCode, Message));
	TestEqual(
		TEXT("select missing result proof error"),
		ErrorCode,
		FString(TEXT("select_result_type_unresolved")));

	SelectRequest.ContextEvidence.Add(TEXT("generic.select.result_type_proof"), StaticEnum<ETickingGroup>()->GetPathName());
	TestTrue(
		TEXT("select accepts result type proof without struct path"),
		FBlueprintHelperStructFieldPolicyEvidenceReader::Read(SelectRequest, Evidence, ErrorCode, Message));
	TestTrue(TEXT("select result proof retained"), Evidence.ResultTypeProof.Contains(TEXT("ETickingGroup")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsStructSelectBuilderBoundaryTest,
	"BlueprintHelper.GraphWrite.GenericOps.StructSelect.StructFieldBuilderBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGenericOpsStructSelectBuilderBoundaryTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("make_struct supported"), FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(TEXT("make_struct")));
	TestTrue(TEXT("break_struct supported"), FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(TEXT("break_struct")));
	TestTrue(TEXT("set_fields_in_struct supported"), FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(TEXT("set_fields_in_struct")));
	TestFalse(TEXT("split_pin is not a struct field builder operation"), FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(TEXT("split_pin")));
	TestFalse(TEXT("recombine_pin is not a struct field builder operation"), FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(TEXT("recombine_pin")));

	const TArray<FString> SetFieldsKeys =
		FBlueprintHelperStructFieldFragmentBuilder::RequiredEvidenceKeys(TEXT("set_fields_in_struct"));
	TestTrue(TEXT("set_fields requires struct path"), SetFieldsKeys.Contains(TEXT("generic.struct.struct_path")));
	TestTrue(TEXT("set_fields requires selected field paths"), SetFieldsKeys.Contains(TEXT("generic.struct.selected_field_paths")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsSetFieldsBuildsActualNodeTest,
	"BlueprintHelper.GraphWrite.GenericOps.StructSelect.SetFieldsActualNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGenericOpsSetFieldsBuildsActualNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeStructSelectBlueprint();
	UEdGraph* Graph = GetStructSelectGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeStructSelectEvidenceRequest(TEXT("set_fields_in_struct"));
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.ContextEvidence.Add(TEXT("generic.struct.selected_field_paths"), TEXT("X"));

	FBlueprintHelperNodeFragment Fragment;
	FString Error;
	const bool bBuilt = FBlueprintHelperStructFieldFragmentBuilder::BuildSetFieldsInStructFragment(
		Graph,
		Request,
		Fragment,
		Error);
	TestTrue(TEXT("set_fields_in_struct builds a fragment"), bBuilt);
	if (!bBuilt)
	{
		AddError(Error);
		return false;
	}

	TestNotNull(TEXT("fragment primary node"), Fragment.PrimaryNode);
	TestNotNull(TEXT("actual set fields node"), Cast<UK2Node_SetFieldsInStruct>(Fragment.PrimaryNode));
	TestTrue(TEXT("selected field data input exists"), Fragment.DataInputs.Contains(TEXT("X")));
	TestEqual(
		TEXT("generic struct ownership tag"),
		Fragment.OwnershipTags.FindRef(TEXT("generic.struct.operation")),
		FString(TEXT("set_fields_in_struct")));
	TestEqual(
		TEXT("selected fields ownership tag"),
		Fragment.OwnershipTags.FindRef(TEXT("generic.struct.selected_field_paths")),
		FString(TEXT("X")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsSelectBuilderResultProofTest,
	"BlueprintHelper.GraphWrite.GenericOps.StructSelect.SelectResultProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGenericOpsSelectBuilderResultProofTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestFalse(TEXT("wildcard result proof fails"), BuildSelectWithResultProof(TEXT("wildcard"), Error));
	TestTrue(TEXT("wildcard error is explicit"), Error.Contains(TEXT("wildcard_residual")));

	const FString ValidProofs[] = {
		TEXT("int"),
		FString::Printf(TEXT("enum:%s"), *StaticEnum<ETickingGroup>()->GetPathName()),
		FString::Printf(TEXT("object:%s"), *AActor::StaticClass()->GetPathName()),
		FString::Printf(TEXT("class:%s"), *AActor::StaticClass()->GetPathName()),
		FString::Printf(TEXT("soft_object:%s"), *AActor::StaticClass()->GetPathName()),
		FString::Printf(TEXT("soft_class:%s"), *AActor::StaticClass()->GetPathName()),
		FString::Printf(TEXT("interface:%s"), *UInterface::StaticClass()->GetPathName())
	};

	for (const FString& Proof : ValidProofs)
	{
		Error.Reset();
		TestFalse(*FString::Printf(TEXT("proof %s still cannot build without action result"), *Proof), BuildSelectWithResultProof(Proof, Error));
		TestTrue(
			*FString::Printf(TEXT("proof %s passes type validation"), *Proof),
			Error.Contains(TEXT("action provider did not resolve")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsSplitRecombineStatementRejectedTest,
	"BlueprintHelper.GraphWrite.GenericOps.StructSelect.SplitRecombineRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGenericOpsSplitRecombineStatementRejectedTest::RunTest(const FString& Parameters)
{
	for (const FString& Kind : { FString(TEXT("split_pin")), FString(TEXT("recombine_pin")) })
	{
		FBlueprintHelperGraphSemanticIR IR;
		const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
			MakeLogicSpecWithStatementKind(Kind),
			IR);

		TestFalse(*FString::Printf(TEXT("%s has parse errors"), *Kind), bBuilt);
		TestTrue(*FString::Printf(TEXT("%s reports unsupported statement kind"), *Kind), HasDiagnosticCode(IR, TEXT("statement_kind_unsupported")));
	}
	return true;
}

#endif
