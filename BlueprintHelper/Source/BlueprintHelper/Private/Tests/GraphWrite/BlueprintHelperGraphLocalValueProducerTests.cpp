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
	const FBlueprintHelperGraphSemanticContext* Context = nullptr,
	const bool bTreatDiagnosticsAsErrors = true)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	const bool bBuilt = Context
		? FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, *Context, OutIR)
		: FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, OutIR);
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
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, VoidCallGraphLocalValueSpec(), IR, nullptr, false))
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerFieldAccessKeepsGetterFragmentTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.FieldAccessKeepsGetterFragment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerFieldAccessKeepsGetterFragmentTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_create_widget",
			"kind": "create",
			"create_operation": "create_widget",
			"class_path": "/Script/UMG.UserWidget",
			"value_type": "/Script/UMG.UserWidget",
			"result_symbol": "CreatedVitalsHud",
			"args": {}
		}, {
			"id": "stmt_set_percent",
			"kind": "call",
			"target": "SetPercent",
			"target_object": {
				"kind": "field",
				"field_operation": "get",
				"field_scope": "field_access",
				"target": "CreatedVitalsHud",
				"property_path": "StaminaBar",
				"context_evidence": {
					"field_owner_class": "/Script/UMG.UserWidget"
				}
			},
			"args": {
				"InPercent": { "kind": "literal", "value_type": "float", "value": 1.0 }
			}
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	TestTrue(TEXT("call target_object exists"), IR.Statements.Num() == 2 && IR.Statements[1]->TargetObject.IsValid());
	if (IR.Statements.Num() != 2 || !IR.Statements[1]->TargetObject.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("field_access target stays a field expression"),
		IR.Statements[1]->TargetObject->FieldScope,
		FString(TEXT("field_access")));
	TestEqual(
		TEXT("field_access expression does not inherit owner object as return type"),
		IR.Statements[1]->TargetObject->Type,
		FString());

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("dag has no errors"), Dag.HasErrors());

	FString FieldAccessFragmentId;
	for (const FBlueprintHelperGraphFragmentRef& Fragment : Dag.Fragments)
	{
		if (Fragment.Kind == TEXT("expr_get_field_access"))
		{
			FieldAccessFragmentId = Fragment.FragmentId;
			break;
		}
	}
	TestFalse(TEXT("field_access emits a getter fragment"), FieldAccessFragmentId.IsEmpty());

	const bool bWidgetFeedsGetterSelf = Dag.DataEdges.ContainsByPredicate(
		[&FieldAccessFragmentId](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_create_widget")
				&& Edge.From.PinName == TEXT("value")
				&& Edge.To.FragmentId == FieldAccessFragmentId
				&& Edge.To.PinName == TEXT("self");
		});

	TestTrue(TEXT("create_widget result feeds field_access getter self pin"), bWidgetFeedsGetterSelf);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerFieldAccessMemberTargetFeedsGetterSelfTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.FieldAccessMemberTargetFeedsGetterSelf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerFieldAccessMemberTargetFeedsGetterSelfTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_set_percent",
			"kind": "call",
			"target": "SetPercent",
			"target_object": {
				"id": "VitalsHudWidget_StaminaBar",
				"kind": "field",
				"field_operation": "get",
				"field_scope": "field_access",
				"target": "VitalsHudWidget",
				"property_path": "StaminaBar",
				"context_evidence": {
					"field_owner_class": "/Game/ThirdPerson/UI/WBP_ThirdPersonVitalsHUD.WBP_ThirdPersonVitalsHUD_C"
				}
			},
			"args": {
				"InPercent": { "kind": "literal", "value_type": "float", "value": 0.5 }
			}
		}]
	})JSON");

	FBlueprintHelperGraphSemanticContext Context;
	Context.VariableNames.Add(TEXT("vitalshudwidget"));
	Context.TargetTypes.Add(
		TEXT("vitalshudwidget"),
		TEXT("/Game/ThirdPerson/UI/WBP_ThirdPersonVitalsHUD.WBP_ThirdPersonVitalsHUD_C"));

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR, &Context))
	{
		return false;
	}

	TestTrue(TEXT("call target_object exists"), IR.Statements.Num() == 1 && IR.Statements[0]->TargetObject.IsValid());
	if (IR.Statements.Num() != 1 || !IR.Statements[0]->TargetObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& FieldAccess = IR.Statements[0]->TargetObject;
	TestEqual(TEXT("field_access reads requested widget member"), FieldAccess->ResolvedTarget.Member, FString(TEXT("StaminaBar")));
	TestEqual(TEXT("field_access keeps property path"), FieldAccess->ResolvedTarget.PropertyPath, FString(TEXT("StaminaBar")));
	TestTrue(TEXT("field_access synthesizes owner getter"), FieldAccess->TargetObject.IsValid());
	if (!FieldAccess->TargetObject.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("owner getter targets widget variable"), FieldAccess->TargetObject->Target, FString(TEXT("VitalsHudWidget")));
	TestEqual(
		TEXT("owner getter carries widget class"),
		FieldAccess->TargetObject->Type,
		FString(TEXT("/Game/ThirdPerson/UI/WBP_ThirdPersonVitalsHUD.WBP_ThirdPersonVitalsHUD_C")));

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("dag has no errors"), Dag.HasErrors());

	const bool bWidgetVariableFeedsGetterSelf = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("VitalsHudWidget_StaminaBar__target_object")
				&& Edge.From.PinName == TEXT("value")
				&& Edge.To.FragmentId == TEXT("VitalsHudWidget_StaminaBar")
				&& Edge.To.PinName == TEXT("self");
		});

	TestTrue(TEXT("widget member getter feeds field_access self pin"), bWidgetVariableFeedsGetterSelf);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerFieldAccessStatementTargetFeedsSetterSelfTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.FieldAccessStatementTargetFeedsSetterSelf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerFieldAccessStatementTargetFeedsSetterSelfTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_create_save",
			"kind": "create",
			"create_operation": "construct_object",
			"class_path": "/Game/ThirdPerson/Blueprints/BP_ThirdPersonHealthSaveGame.BP_ThirdPersonHealthSaveGame_C",
			"value_type": "/Game/ThirdPerson/Blueprints/BP_ThirdPersonHealthSaveGame.BP_ThirdPersonHealthSaveGame_C",
			"result_symbol": "CreatedVitalsSave",
			"args": {}
		}, {
			"id": "stmt_set_saved_health",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "field_access",
			"target": "CreatedVitalsSave",
			"property_path": "SavedHealth",
			"value": { "kind": "literal", "value_type": "float", "value": 75.0 },
			"context_evidence": {
				"field_owner_class": "/Game/ThirdPerson/Blueprints/BP_ThirdPersonHealthSaveGame.BP_ThirdPersonHealthSaveGame_C"
			}
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	TestTrue(TEXT("field statement exists"), IR.Statements.Num() == 2 && IR.Statements[1].IsValid());
	if (IR.Statements.Num() != 2 || !IR.Statements[1].IsValid())
	{
		return false;
	}
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& FieldStatement = IR.Statements[1];
	TestEqual(TEXT("field_access statement target is temporary"), FieldStatement->ResolvedTarget.Kind, EBlueprintHelperGraphTargetKind::Temporary);
	TestEqual(TEXT("field_access statement reads requested member"), FieldStatement->ResolvedTarget.Member, FString(TEXT("SavedHealth")));
	TestEqual(TEXT("field_access statement keeps property path"), FieldStatement->ResolvedTarget.PropertyPath, FString(TEXT("SavedHealth")));

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("dag has no errors"), Dag.HasErrors());

	const bool bSaveObjectFeedsSetterSelf = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_create_save")
				&& Edge.From.PinName == TEXT("value")
				&& Edge.To.FragmentId == TEXT("stmt_set_saved_health")
				&& Edge.To.PinName == TEXT("self");
		});

	TestTrue(TEXT("create result_symbol feeds field_access setter self pin"), bSaveObjectFeedsSetterSelf);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerCreateResultSymbolCarriesClassPathTypeTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.CreateResultSymbolCarriesClassPathType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerCreateResultSymbolCarriesClassPathTypeTest::RunTest(const FString& Parameters)
{
	const FString SaveClassPath = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonHealthSaveGame.BP_ThirdPersonHealthSaveGame_C");
	const FString Json = FString::Printf(TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_create_save",
			"kind": "create",
			"create_operation": "construct_object",
			"class_path": "%s",
			"result_symbol": "CreatedVitalsSave",
			"args": {}
		}, {
			"id": "stmt_set_saved_health",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "field_access",
			"target": "CreatedVitalsSave",
			"property_path": "SavedHealth",
			"value": { "kind": "literal", "value_type": "float", "value": 75.0 },
			"context_evidence": {
				"field_owner_class": "%s"
			}
		}]
	})JSON"), *SaveClassPath, *SaveClassPath);

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	FBlueprintHelperGraphSymbol CreatedSymbol;
	TestTrue(TEXT("created save symbol registered"), IR.TryFindSymbol(TEXT("CreatedVitalsSave"), CreatedSymbol));
	TestEqual(TEXT("create result_symbol uses class_path as type"), CreatedSymbol.Type, SaveClassPath);
	TestTrue(TEXT("field statement exists"), IR.Statements.Num() == 2 && IR.Statements[1].IsValid());
	if (IR.Statements.Num() != 2 || !IR.Statements[1].IsValid())
	{
		return false;
	}
	TestEqual(TEXT("field_access owner keeps class type"), IR.Statements[1]->ResolvedTarget.Type, SaveClassPath);
	return true;
}

#endif
