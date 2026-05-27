#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

namespace
{
static bool BuildLogicSpecForGraphLocalValueProducer(
	FAutomationTestBase& Test,
	const FString& Json,
	FBlueprintHelperGraphSemanticIR& OutIR,
	const bool bTreatDiagnosticsAsErrors = true)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, OutIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : OutIR.Diagnostics)
	{
		if (bTreatDiagnosticsAsErrors && Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(TEXT("%s at %s: %s"), *Diagnostic.Code, *Diagnostic.Path, *Diagnostic.Message));
		}
	}
	return bTreatDiagnosticsAsErrors
		? Test.TestTrue(TEXT("semantic ir builds"), bBuilt)
		: true;
}

static FString TimerHandleCallGraphLocalValueSpec()
{
	return TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_make_handle",
			"kind": "call",
			"target": "/Script/Engine.KismetSystemLibrary:K2_SetTimer",
			"value_type": "TimerHandle",
			"result_symbol": "LoopTimerHandle",
			"args": {
				"FunctionName": { "kind": "literal", "value_type": "string", "value": "HandlePulse" },
				"Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
				"bLooping": { "kind": "literal", "value_type": "bool", "value": true }
			}
		}, {
			"id": "stmt_cache_handle",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "variable",
			"target": "LoopDoorTimerHandle",
			"value": { "kind": "get", "name": "LoopTimerHandle" }
		}]
	})JSON");
}

static FString VoidCallGraphLocalValueSpec()
{
	return TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_print",
			"kind": "call",
			"target": "/Script/Engine.KismetSystemLibrary:PrintString",
			"result_symbol": "VoidResult",
			"args": {
				"InString": { "kind": "literal", "value_type": "string", "value": "Pulse" }
			}
		}]
	})JSON");
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerSemanticIRTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.SemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerSemanticIRTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, TimerHandleCallGraphLocalValueSpec(), IR))
	{
		return false;
	}

	TestEqual(TEXT("two statements"), IR.Statements.Num(), 2);
	TestTrue(TEXT("consumer value exists"), IR.Statements[1]->Value.IsValid());
	TestEqual(
		TEXT("consumer value is temporary"),
		IR.Statements[1]->Value->ResolvedTarget.Kind,
		EBlueprintHelperGraphTargetKind::Temporary);
	TestEqual(
		TEXT("temporary type is carried"),
		IR.Statements[1]->Value->ResolvedTarget.Type,
		FString(TEXT("TimerHandle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerRejectsVoidCallResultSymbolTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.RejectsVoidCallResultSymbol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerRejectsVoidCallResultSymbolTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, VoidCallGraphLocalValueSpec(), IR, false))
	{
		return false;
	}

	const bool bHasMissingOutputDiagnostic = IR.Diagnostics.ContainsByPredicate(
		[](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == TEXT("result_symbol_missing_output_type");
		});
	FBlueprintHelperGraphSymbol Symbol;

	TestTrue(TEXT("void call result_symbol reports missing output type"), bHasMissingOutputDiagnostic);
	TestFalse(TEXT("void call result_symbol is not registered"), IR.TryFindSymbol(TEXT("VoidResult"), Symbol));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerActionContextTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.ActionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerActionContextTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, TimerHandleCallGraphLocalValueSpec(), IR))
	{
		return false;
	}

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromSemanticIR(IR);

	const bool bHasTemporaryFieldDemand = Demands.ContainsByPredicate(
		[](const FBlueprintHelperActionContextDemand& Demand)
		{
			return Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Field
				&& Demand.TargetPath.Equals(TEXT("LoopTimerHandle"), ESearchCase::IgnoreCase);
		});

	TestFalse(TEXT("temporary get does not create field variable demand"), bHasTemporaryFieldDemand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerFragmentDagTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.FragmentDag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerFragmentDagTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, TimerHandleCallGraphLocalValueSpec(), IR))
	{
		return false;
	}

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("dag has no errors"), Dag.HasErrors());

	const bool bHasReturnValueEdge = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_make_handle")
				&& Edge.From.PinName == TEXT("ReturnValue")
				&& Edge.To.FragmentId == TEXT("stmt_cache_handle")
				&& Edge.To.PinName == TEXT("value");
		});

	TestTrue(TEXT("call result_symbol feeds later field set"), bHasReturnValueEdge);
	return true;
}

#endif
