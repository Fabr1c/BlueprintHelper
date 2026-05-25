#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace
{
static bool ParseContainerActionLogicSpec(
	FAutomationTestBase& Test,
	const FString& Json,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	TArray<FBlueprintHelperGraphSemanticDiagnostic> Diagnostics;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, OutIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : OutIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(TEXT("%s at %s: %s"), *Diagnostic.Code, *Diagnostic.Path, *Diagnostic.Message));
		}
	}
	return Test.TestTrue(TEXT("semantic ir builds"), bBuilt);
}

static bool BuildContainerActionLogicSpec(
	FAutomationTestBase& Test,
	const FString& Json,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}
	return FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, OutIR);
}

static bool HasContainerActionDiagnostic(
	const FBlueprintHelperGraphSemanticIR& IR,
	const FString& Code)
{
	return IR.Diagnostics.ContainsByPredicate(
		[&Code](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
		{
			return Diagnostic.Code.Equals(Code, ESearchCase::IgnoreCase);
		});
}

static UFunction* ResolveFunctionQuery(const FString& FunctionQuery)
{
	int32 ColonIndex = INDEX_NONE;
	if (!FunctionQuery.FindLastChar(TEXT(':'), ColonIndex) || ColonIndex <= 0 || ColonIndex >= FunctionQuery.Len() - 1)
	{
		return nullptr;
	}

	const FString OwnerPath = FunctionQuery.Left(ColonIndex).TrimStartAndEnd();
	const FString FunctionName = FunctionQuery.Mid(ColonIndex + 1).TrimStartAndEnd();
	if (OwnerPath.IsEmpty() || FunctionName.IsEmpty())
	{
		return nullptr;
	}

	UClass* OwnerClass = FindObject<UClass>(nullptr, *OwnerPath);
	if (!OwnerClass)
	{
		OwnerClass = LoadObject<UClass>(nullptr, *OwnerPath);
	}
	return OwnerClass ? OwnerClass->FindFunctionByName(FName(*FunctionName)) : nullptr;
}

static FString MakeContainerActionObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeContainerActionBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperContainerAction/%s"),
		*MakeContainerActionObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeContainerActionObjectName(TEXT("BP_ContainerAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperContainerActionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* FindContainerActionEventGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FEdGraphTerminalType MakeContainerActionTerminalType(const FName Category)
{
	FEdGraphTerminalType TerminalType;
	TerminalType.TerminalCategory = Category;
	return TerminalType;
}

static FEdGraphPinType MakeContainerActionPinType(
	const FName Category,
	const EPinContainerType ContainerType,
	const FEdGraphTerminalType& ValueType = FEdGraphTerminalType())
{
	return FEdGraphPinType(Category, NAME_None, nullptr, ContainerType, false, ValueType);
}

static bool AddContainerActionVariable(UBlueprint* Blueprint, const FString& Name, const FEdGraphPinType& Type)
{
	if (!Blueprint || !FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), Type))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

static FString MakeContainerActionLogicJson(const FString& StatementJson)
{
	return FString::Printf(TEXT(R"JSON({
		"logic_spec": {
			"schema": "BlueprintLogicSpec.v2",
			"statements": [%s]
		}
	})JSON"), *StatementJson);
}

static bool RunContainerActionFixture(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& TestName,
	const FString& StatementJson,
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
{
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(Graph, MakeContainerActionLogicJson(StatementJson), Unresolved);
	if (!Test.TestTrue(*FString::Printf(TEXT("%s generation succeeds"), *TestName), Result.bSucceed))
	{
		for (const TSharedPtr<FUnresolvedNodeItem>& Item : Unresolved)
		{
			if (Item.IsValid())
			{
				Test.AddError(FString::Printf(TEXT("%s unresolved: %s - %s"), *TestName, *Item->DisplayText, *Item->Reason));
			}
		}
		return false;
	}
	if (!Test.TestEqual(*FString::Printf(TEXT("%s connection diagnostics"), *TestName), Result.ConnectionDiagnostics.Num(), 0))
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.ConnectionDiagnostics)
		{
			Test.AddError(FString::Printf(TEXT("%s connection diagnostic: %s"), *TestName, *Diagnostic.Message));
		}
		return false;
	}
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s creates at least one connection"), *TestName),
		Result.RequestedConnectionCount > 0 && Result.CreatedConnectionCount > 0))
	{
		Test.AddError(FString::Printf(
			TEXT("%s connection counts: requested=%d created=%d generated_nodes=%d"),
			*TestName,
			Result.RequestedConnectionCount,
			Result.CreatedConnectionCount,
			Result.GeneratedNodeCount));
		return false;
	}

	FString ReadbackFailure;
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s readback passes"), *TestName),
		FBlueprintHelperContainerActionReadbackVerifier::Verify(Blueprint, Graph, Expectation, ReadbackFailure)))
	{
		Test.AddError(ReadbackFailure);
		return false;
	}
	return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionSemanticIRTest,
	"BlueprintHelper.GraphWrite.ContainerAction.SemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionSemanticIRTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_add",
			"kind": "container_action",
			"container_kind": "array",
			"container_operation": "add",
			"target": { "kind": "get", "name": "Items" },
			"item": { "kind": "literal", "value": 7 },
			"element_type": "int"
		}, {
			"id": "stmt_result",
			"kind": "let",
			"name": "HasScore",
			"value": {
				"kind": "container_action",
				"container_kind": "map",
				"container_operation": "contains",
				"target": { "kind": "get", "name": "Scores" },
				"key": { "kind": "literal", "value": "PlayerA" },
				"key_type": "string",
				"value_type": "int"
			}
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!ParseContainerActionLogicSpec(*this, Json, IR))
	{
		return false;
	}

	TestEqual(TEXT("statement count"), IR.Statements.Num(), 2);
	if (IR.Statements.Num() < 2 || !IR.Statements[0].IsValid() || !IR.Statements[1].IsValid())
	{
		return false;
	}

	const FBlueprintHelperGraphStatementIR& Statement = *IR.Statements[0];
	TestEqual(TEXT("statement kind"), Statement.Kind, EBlueprintHelperGraphStatementKind::ContainerAction);
	TestEqual(TEXT("statement pattern"), Statement.PatternName, FString(TEXT("container_action")));
	TestEqual(TEXT("container kind"), Statement.ContainerKind, FString(TEXT("array")));
	TestEqual(TEXT("container operation"), Statement.ContainerOperation, FString(TEXT("add")));
	TestEqual(TEXT("element type"), Statement.ElementType, FString(TEXT("int")));
	TestEqual(TEXT("element type alias"), Statement.PinType, FString(TEXT("int")));
	TestTrue(TEXT("target role expression"), Statement.TargetObject.IsValid());
	TestTrue(TEXT("item role expression"), Statement.Args.Contains(TEXT("item")));

	const TSharedPtr<FBlueprintHelperGraphExpressionIR> Expression = IR.Statements[1]->Value;
	TestTrue(TEXT("container expression present"), Expression.IsValid());
	if (!Expression.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("expression kind"), Expression->Kind, EBlueprintHelperGraphExpressionKind::ContainerAction);
	TestEqual(TEXT("expression pattern"), Expression->PatternName, FString(TEXT("container_action")));
	TestEqual(TEXT("expression container kind"), Expression->ContainerKind, FString(TEXT("map")));
	TestEqual(TEXT("expression container operation"), Expression->ContainerOperation, FString(TEXT("contains")));
	TestEqual(TEXT("key type"), Expression->KeyType, FString(TEXT("string")));
	TestEqual(TEXT("value type"), Expression->ValueType, FString(TEXT("int")));
	TestEqual(TEXT("expression result type"), Expression->Type, FString(TEXT("bool")));
	TestTrue(TEXT("expression target role"), Expression->TargetObject.IsValid());
	TestTrue(TEXT("expression key role"), Expression->Args.Contains(TEXT("key")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionContractValidationTest,
	"BlueprintHelper.GraphWrite.ContainerAction.ContractValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionContractValidationTest::RunTest(const FString& Parameters)
{
	{
		const FString Json = TEXT(R"JSON({
			"schema": "BlueprintLogicSpec.v2",
			"statements": [{
				"id": "stmt_foreach",
				"kind": "container_action",
				"container_kind": "array",
				"container_operation": "foreach",
				"target": { "kind": "get", "name": "Items" }
			}]
		})JSON");
		FBlueprintHelperGraphSemanticIR IR;
		TestFalse(TEXT("unsupported operation is rejected"), BuildContainerActionLogicSpec(*this, Json, IR));
		TestTrue(TEXT("unsupported operation diagnostic"), HasContainerActionDiagnostic(IR, TEXT("unsupported_container_operation")));
	}

	{
		const FString Json = TEXT(R"JSON({
			"schema": "BlueprintLogicSpec.v2",
			"statements": [{
				"id": "stmt_missing_item",
				"kind": "container_action",
				"container_kind": "array",
				"container_operation": "add",
				"target": { "kind": "get", "name": "Items" },
				"element_type": "int"
			}]
		})JSON");
		FBlueprintHelperGraphSemanticIR IR;
		TestFalse(TEXT("missing required role is rejected"), BuildContainerActionLogicSpec(*this, Json, IR));
		TestTrue(TEXT("missing required role diagnostic"), HasContainerActionDiagnostic(IR, TEXT("container_role_missing")));
	}

	{
		const FString Json = TEXT(R"JSON({
			"schema": "BlueprintLogicSpec.v2",
			"statements": [{
				"id": "stmt_mutating_result",
				"kind": "container_action",
				"container_kind": "array",
				"container_operation": "add",
				"target": { "kind": "get", "name": "Items" },
				"item": { "kind": "literal", "value": 7, "type": "int" },
				"element_type": "int",
				"result_symbol": "InvalidResult"
			}]
		})JSON");
		FBlueprintHelperGraphSemanticIR IR;
		TestFalse(TEXT("mutating result_symbol is rejected"), BuildContainerActionLogicSpec(*this, Json, IR));
		TestTrue(TEXT("mutating result_symbol diagnostic"), HasContainerActionDiagnostic(IR, TEXT("container_result_symbol_invalid")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionFragmentDagTest,
	"BlueprintHelper.GraphWrite.ContainerAction.FragmentDag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionFragmentDagTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_has_tag",
			"kind": "container_action",
			"container_kind": "set",
			"container_operation": "contains",
			"target": { "kind": "get", "name": "Tags" },
			"item": { "kind": "literal", "value": "Ready" },
			"element_type": "string",
			"result_symbol": "bHasReady"
		}, {
			"id": "stmt_use_tag",
			"kind": "let",
			"name": "CachedReady",
			"value": { "kind": "get", "name": "bHasReady" }
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!ParseContainerActionLogicSpec(*this, Json, IR))
	{
		return false;
	}

	FBlueprintHelperGraphFragmentDag Dag;
	const bool bDagBuilt = FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag);
	TestTrue(TEXT("container action fragment dag builds"), bDagBuilt);
	TestFalse(TEXT("container action dag has no errors"), Dag.HasErrors());

	FBlueprintHelperGraphFragmentRef ContainerFragment;
	TestTrue(TEXT("container action fragment exists"), Dag.TryFindFragment(TEXT("stmt_has_tag"), ContainerFragment));
	TestEqual(TEXT("container action fragment kind"), ContainerFragment.Kind, FString(TEXT("statement_container_action")));
	TestEqual(TEXT("container action metadata kind"), ContainerFragment.Metadata.FindRef(TEXT("container_kind")), FString(TEXT("set")));
	TestEqual(TEXT("container action metadata operation"), ContainerFragment.Metadata.FindRef(TEXT("container_operation")), FString(TEXT("contains")));

	const bool bHasTargetEdge = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.To.FragmentId == TEXT("stmt_has_tag")
				&& Edge.To.PinName == TEXT("target");
		});
	const bool bHasItemEdge = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.To.FragmentId == TEXT("stmt_has_tag")
				&& Edge.To.PinName == TEXT("item");
		});
	const bool bHasResultConsumerEdge = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_has_tag")
				&& Edge.From.PinName == TEXT("result")
				&& Edge.From.Type == TEXT("bool")
				&& Edge.To.PinName == TEXT("value");
		});

	TestTrue(TEXT("target role has data edge"), bHasTargetEdge);
	TestTrue(TEXT("item role has data edge"), bHasItemEdge);
	TestTrue(TEXT("result_symbol can feed following statement"), bHasResultConsumerEdge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionArrayResultFragmentDagTest,
	"BlueprintHelper.GraphWrite.ContainerAction.ArrayResultFragmentDag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionArrayResultFragmentDagTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_tags_to_array",
			"kind": "container_action",
			"container_kind": "set",
			"container_operation": "to_array",
			"target": { "kind": "get", "name": "TagSet" },
			"element_type": "string",
			"result_symbol": "TagArray"
		}, {
			"id": "stmt_tag_count",
			"kind": "container_action",
			"container_kind": "array",
			"container_operation": "length",
			"target": { "kind": "get", "name": "TagArray" },
			"element_type": "string",
			"result_symbol": "TagCount"
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!ParseContainerActionLogicSpec(*this, Json, IR))
	{
		return false;
	}

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("array result dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("array result dag has no errors"), Dag.HasErrors());

	const FBlueprintHelperGraphFragmentDataEdge* ArrayResultEdge = Dag.DataEdges.FindByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_tags_to_array")
				&& Edge.From.PinName == TEXT("result")
				&& Edge.To.FragmentId == TEXT("stmt_tag_count")
				&& Edge.To.PinName == TEXT("target");
		});
	TestNotNull(TEXT("set.to_array result feeds array.length target"), ArrayResultEdge);
	if (!ArrayResultEdge)
	{
		return false;
	}
	TestEqual(TEXT("array result category"), ArrayResultEdge->From.PinType.Category, FString(TEXT("string")));
	TestEqual(TEXT("array result container type"), ArrayResultEdge->From.PinType.ContainerType, FString(TEXT("array")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionFocusedE2ETest,
	"BlueprintHelper.GraphWrite.ContainerAction.FocusedE2E",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionEndpointPinTypeJsonRoundTripTest,
	"BlueprintHelper.GraphWrite.ContainerAction.EndpointPinTypeJsonRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionEndpointPinTypeJsonRoundTripTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphFragmentEndpointRef SourceEndpoint;
	SourceEndpoint.FragmentId = TEXT("stmt_tags_to_array");
	SourceEndpoint.PortId = TEXT("result");
	SourceEndpoint.PinName = TEXT("result");
	SourceEndpoint.Type = TEXT("string");
	SourceEndpoint.Direction = EBlueprintHelperGraphFragmentPortDirection::DataOutput;
	SourceEndpoint.PinType.Category = TEXT("string");
	SourceEndpoint.PinType.ContainerType = TEXT("array");
	SourceEndpoint.PinType.bIsConst = true;

	const TSharedRef<FJsonObject> EndpointJson = SourceEndpoint.ToJson();
	const FBlueprintHelperGraphFragmentEndpointRef RoundTripEndpoint =
		FBlueprintHelperGraphFragmentEndpointRef::FromJson(EndpointJson);

	TestEqual(TEXT("round-trip fragment id"), RoundTripEndpoint.FragmentId, SourceEndpoint.FragmentId);
	TestEqual(TEXT("round-trip port id"), RoundTripEndpoint.PortId, SourceEndpoint.PortId);
	TestEqual(TEXT("round-trip pin name"), RoundTripEndpoint.PinName, SourceEndpoint.PinName);
	TestEqual(TEXT("round-trip type"), RoundTripEndpoint.Type, SourceEndpoint.Type);
	TestEqual(TEXT("round-trip direction"), RoundTripEndpoint.Direction, SourceEndpoint.Direction);
	TestEqual(TEXT("round-trip pin category"), RoundTripEndpoint.PinType.Category, FString(TEXT("string")));
	TestEqual(TEXT("round-trip pin container"), RoundTripEndpoint.PinType.ContainerType, FString(TEXT("array")));
	TestTrue(TEXT("round-trip const flag"), RoundTripEndpoint.PinType.bIsConst);
	return true;
}

bool FBlueprintHelperGraphWriteContainerActionFocusedE2ETest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeContainerActionBlueprint();
	UEdGraph* Graph = FindContainerActionEventGraph(Blueprint);
	TestNotNull(TEXT("container action blueprint"), Blueprint);
	TestNotNull(TEXT("container action graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FEdGraphPinType ItemsType =
		MakeContainerActionPinType(UEdGraphSchema_K2::PC_Int, EPinContainerType::Array);
	const FEdGraphPinType ScoresType =
		MakeContainerActionPinType(
			UEdGraphSchema_K2::PC_String,
			EPinContainerType::Map,
			MakeContainerActionTerminalType(UEdGraphSchema_K2::PC_Int));
	const FEdGraphPinType TagsType =
		MakeContainerActionPinType(UEdGraphSchema_K2::PC_String, EPinContainerType::Set);

	TestTrue(TEXT("Items variable added"), AddContainerActionVariable(Blueprint, TEXT("Items"), ItemsType));
	TestTrue(TEXT("Scores variable added"), AddContainerActionVariable(Blueprint, TEXT("Scores"), ScoresType));
	TestTrue(TEXT("TagSet variable added"), AddContainerActionVariable(Blueprint, TEXT("TagSet"), TagsType));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		AddError(TEXT("container action fixture Blueprint failed to compile before generation."));
		return false;
	}

	bool bPassed = true;
	bPassed &= RunContainerActionFixture(
		*this,
		Blueprint,
		Graph,
		TEXT("array add"),
		TEXT(R"JSON({
			"id": "stmt_array_add",
			"kind": "container_action",
			"container_kind": "array",
			"container_operation": "add",
			"target": { "kind": "get", "name": "Items" },
			"item": { "kind": "literal", "value": 7, "type": "int" },
			"element_type": "int"
		})JSON"),
		FBlueprintHelperContainerActionReadbackExpectation{
			TEXT("container.array.add"),
			TEXT("array"),
			TEXT("add"),
			TEXT("Items"),
			TEXT("int"),
			FString(),
			FString(),
			{ TEXT("target"), TEXT("item") },
			false,
			false });

	bPassed &= RunContainerActionFixture(
		*this,
		Blueprint,
		Graph,
		TEXT("map contains"),
		TEXT(R"JSON({
			"id": "stmt_map_contains",
			"kind": "container_action",
			"container_kind": "map",
			"container_operation": "contains",
			"target": { "kind": "get", "name": "Scores" },
			"key": { "kind": "literal", "value": "PlayerA", "type": "string" },
			"key_type": "string",
			"value_type": "int",
			"result_symbol": "bHasScore"
		})JSON"),
		FBlueprintHelperContainerActionReadbackExpectation{
			TEXT("container.map.contains"),
			TEXT("map"),
			TEXT("contains"),
			TEXT("Scores"),
			FString(),
			TEXT("string"),
			TEXT("int"),
			{ TEXT("target"), TEXT("key") },
			false,
			true });
	{
		FString WrongTypeFailure;
		FBlueprintHelperContainerActionReadbackExpectation WrongTypeExpectation{
			TEXT("container.map.contains"),
			TEXT("map"),
			TEXT("contains"),
			TEXT("Scores"),
			FString(),
			TEXT("name"),
			TEXT("float"),
			{ TEXT("target"), TEXT("key") },
			false,
			true };
		bPassed &= TestFalse(
			TEXT("map contains readback rejects wrong key/value type expectation"),
			FBlueprintHelperContainerActionReadbackVerifier::Verify(Blueprint, Graph, WrongTypeExpectation, WrongTypeFailure));
	}

	bPassed &= RunContainerActionFixture(
		*this,
		Blueprint,
		Graph,
		TEXT("set to array"),
		TEXT(R"JSON({
			"id": "stmt_set_to_array",
			"kind": "container_action",
			"container_kind": "set",
			"container_operation": "to_array",
			"target": { "kind": "get", "name": "TagSet" },
			"element_type": "string",
			"result_symbol": "TagArray"
		})JSON"),
		FBlueprintHelperContainerActionReadbackExpectation{
			TEXT("container.set.to_array"),
			TEXT("set"),
			TEXT("to_array"),
			TEXT("TagSet"),
			TEXT("string"),
			FString(),
			FString(),
			{ TEXT("target") },
			false,
			true });

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestFalse(TEXT("container action generated Blueprint compiles"), Blueprint->Status == BS_Error);
	return bPassed && Blueprint->Status != BS_Error;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionContextDemandTest,
	"BlueprintHelper.GraphWrite.ContainerAction.ActionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionContextDemandTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_add",
			"kind": "container_action",
			"container_kind": "array",
			"container_operation": "add",
			"target": { "kind": "get", "name": "Items" },
			"item": { "kind": "literal", "value": 7 },
			"element_type": "int"
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!ParseContainerActionLogicSpec(*this, Json, IR))
	{
		return false;
	}

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromSemanticIR(IR);
	const FBlueprintHelperActionContextDemand* Demand = Demands.FindByPredicate(
		[](const FBlueprintHelperActionContextDemand& Candidate)
		{
			return Candidate.StatementId == TEXT("stmt_add");
		});

	TestNotNull(TEXT("container action demand"), Demand);
	if (!Demand)
	{
		return false;
	}

	TestEqual(TEXT("cluster"), Demand->ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("semantic kind"), Demand->SemanticKind, EBlueprintHelperActionSemanticKind::ContainerAction);
	TestEqual(TEXT("semantic family"), Demand->SemanticFamily, EBlueprintHelperActionSemanticFamily::Callable);
	TestEqual(TEXT("container kind"), Demand->ContainerKind, FString(TEXT("array")));
	TestEqual(TEXT("container operation"), Demand->ContainerOperation, FString(TEXT("add")));
	TestEqual(TEXT("element type"), Demand->ElementType, FString(TEXT("int")));
	TestEqual(TEXT("query"), Demand->Query, FString(TEXT("array.add")));
	TestEqual(TEXT("target path"), Demand->TargetPath, FString(TEXT("Items")));
	TestEqual(TEXT("element argument type"), Demand->ArgumentTypes.FindRef(TEXT("element")), FString(TEXT("int")));
	TestEqual(TEXT("item argument type"), Demand->ArgumentTypes.FindRef(TEXT("item")), FString(TEXT("int")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, *Demand);
	TestEqual(TEXT("context semantic kind"), Context.Semantic.Kind, EBlueprintHelperActionSemanticKind::ContainerAction);
	TestEqual(TEXT("context container kind"), Context.Semantic.ContainerKind, FString(TEXT("array")));
	TestEqual(TEXT("context container operation"), Context.Semantic.ContainerOperation, FString(TEXT("add")));
	TestEqual(TEXT("context element type"), Context.Semantic.ElementType, FString(TEXT("int")));
	TestEqual(TEXT("evidence container kind"), Context.Evidence.FindRef(TEXT("container_kind")), FString(TEXT("array")));
	TestEqual(TEXT("evidence container operation"), Context.Evidence.FindRef(TEXT("container_operation")), FString(TEXT("add")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionVocabularyTest,
	"BlueprintHelper.GraphWrite.ContainerAction.Vocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionVocabularyTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperContainerActionSpec> AllSpecs = FBlueprintHelperContainerActionVocabulary::All();
	TestEqual(TEXT("V1 operation count"), AllSpecs.Num(), 26);
	for (const FBlueprintHelperContainerActionSpec& Spec : AllSpecs)
	{
		TestNotNull(
			FString::Printf(TEXT("function query resolves for %s"), *Spec.OperationId),
			ResolveFunctionQuery(Spec.FunctionQuery));
	}

	const FBlueprintHelperContainerActionSpec* ArrayAdd =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("array"), TEXT("add"));
	TestNotNull(TEXT("array add vocabulary"), ArrayAdd);
	if (ArrayAdd)
	{
		TestEqual(TEXT("array add operation id"), ArrayAdd->OperationId, FString(TEXT("container.array.add")));
		TestTrue(TEXT("array add mutates"), ArrayAdd->bMutatesTarget);
		TestTrue(TEXT("array add requires item"), ArrayAdd->RequiredRoles.Contains(TEXT("item")));
		TestFalse(TEXT("array add does not return value"), ArrayAdd->bReturnsValue);
		TestFalse(TEXT("array add function query"), ArrayAdd->FunctionQuery.IsEmpty());
	}

	const FBlueprintHelperContainerActionSpec* MapContains =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("map"), TEXT("contains"));
	TestNotNull(TEXT("map contains vocabulary"), MapContains);
	if (MapContains)
	{
		TestFalse(TEXT("map contains is query"), MapContains->bMutatesTarget);
		TestTrue(TEXT("map contains returns value"), MapContains->bReturnsValue);
		TestTrue(TEXT("map contains requires key"), MapContains->RequiredRoles.Contains(TEXT("key")));
	}

	const FBlueprintHelperContainerActionSpec* SetToArray =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("set"), TEXT("to_array"));
	TestNotNull(TEXT("set to array vocabulary"), SetToArray);
	if (SetToArray)
	{
		TestTrue(TEXT("set to array returns value"), SetToArray->bReturnsValue);
		TestTrue(TEXT("set to array uses UE function query"), SetToArray->FunctionQuery.Contains(TEXT("BlueprintSetLibrary")));
	}

	TestNull(TEXT("foreach excluded"), FBlueprintHelperContainerActionVocabulary::Find(TEXT("array"), TEXT("foreach")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionResolverTest,
	"BlueprintHelper.GraphWrite.ContainerAction.Resolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionResolverTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.StatementId = TEXT("container-array-add");
	Request.ProjectedContextHash = TEXT("container-context");
	Request.SemanticConstraintsHash = TEXT("container-semantic");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
	Request.Semantic.ContainerKind = TEXT("array");
	Request.Semantic.ContainerOperation = TEXT("add");
	Request.Semantic.Query = TEXT("array.add");
	Request.Semantic.ElementType = TEXT("int");
	Request.Semantic.ContainerElementPinType.Category = TEXT("int");
	Request.Semantic.ArgumentNames.Add(TEXT("target"));
	Request.Semantic.ArgumentNames.Add(TEXT("item"));
	Request.Semantic.ArgumentTypes.Add(TEXT("element"), TEXT("int"));
	Request.Semantic.ArgumentTypes.Add(TEXT("item"), TEXT("int"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperContainerActionResolver::Resolve(Request);

	TestTrue(
		TEXT("resolved or context-specific invalid"),
		Result.Status == EBlueprintHelperActionResolutionStatus::Resolved
		|| Result.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("resolver cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestNotEqual(TEXT("not asset action"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestNotEqual(TEXT("unsupported operation not returned"), Result.ErrorCode, FString(TEXT("unsupported_container_operation")));
	if (Result.Status == EBlueprintHelperActionResolutionStatus::Resolved)
	{
		TestTrue(TEXT("selected function or spawner exists"), Result.SelectedFunction.IsValid() || Result.SelectedSpawner.IsValid());
		TestTrue(TEXT("candidate describes container"), Result.MatchReason.Contains(TEXT("container.array.add")));
	}

	FBlueprintHelperActionResolutionRequest Unsupported = Request;
	Unsupported.Semantic.ContainerOperation = TEXT("foreach");
	const FBlueprintHelperActionResolutionResult UnsupportedResult =
		FBlueprintHelperContainerActionResolver::Resolve(Unsupported);
	TestEqual(TEXT("unsupported status"), UnsupportedResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("unsupported error"), UnsupportedResult.ErrorCode, FString(TEXT("unsupported_container_operation")));
	TestEqual(TEXT("unsupported cluster"), UnsupportedResult.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	return true;
}

#endif
