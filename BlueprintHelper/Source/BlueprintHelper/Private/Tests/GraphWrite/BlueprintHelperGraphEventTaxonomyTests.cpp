#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
static TSharedPtr<FJsonObject> ParseEventTaxonomyLogicSpec(FAutomationTestBase& Test, const FString& Json)
{
	TSharedPtr<FJsonObject> LogicSpec;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, LogicSpec));
	Test.TestTrue(TEXT("logic spec object valid"), LogicSpec.IsValid());
	return LogicSpec;
}

static bool HasSemanticDiagnostic(
	const FBlueprintHelperGraphSemanticIR& SemanticIR,
	const FString& ExpectedCode)
{
	return SemanticIR.Diagnostics.ContainsByPredicate(
		[&ExpectedCode](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
		{
			return Diagnostic.Code == ExpectedCode;
		});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyCustomEventRequiresSignatureEvidenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent.RequiresSignatureEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyCustomEventRequiresSignatureEvidenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "custom_event",
			"name": "HandleOpened",
			"event_taxonomy": "custom_event",
			"source_cluster": "BlueprintSignature"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	const TSharedPtr<FJsonObject> LogicSpec = ParseEventTaxonomyLogicSpec(*this, Json);

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);

	TestEqual(TEXT("entry kind"), SemanticIR.Entry.Kind, FString(TEXT("custom_event")));
	TestEqual(TEXT("entry taxonomy"), SemanticIR.Entry.EventTaxonomy, FString(TEXT("custom_event")));
	TestTrue(TEXT("missing signature evidence is diagnosed"), HasSemanticDiagnostic(SemanticIR, TEXT("custom_event_signature_evidence_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyCustomEventAcceptsSignatureEvidenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.CustomEvent.AcceptsSignatureEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyCustomEventAcceptsSignatureEvidenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "custom_event",
			"name": "HandleOpened",
			"event_taxonomy": "custom_event",
			"source_cluster": "BlueprintSignature",
			"signature_evidence_id": "signature:custom_event:HandleOpened"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	const TSharedPtr<FJsonObject> LogicSpec = ParseEventTaxonomyLogicSpec(*this, Json);

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);

	TestEqual(TEXT("entry name"), SemanticIR.Entry.Name, FString(TEXT("HandleOpened")));
	TestEqual(TEXT("entry signature evidence"), SemanticIR.Entry.SignatureEvidenceId, FString(TEXT("signature:custom_event:HandleOpened")));
	TestFalse(TEXT("no signature evidence missing diagnostic"), HasSemanticDiagnostic(SemanticIR, TEXT("custom_event_signature_evidence_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyOverrideEntryPreservesReferenceTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.OverrideEntry.PreservesReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyOverrideEntryPreservesReferenceTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"entry": {
			"kind": "event",
			"name": "ReceiveBeginPlay",
			"event_taxonomy": "override_event",
			"source_cluster": "BlueprintSignature",
			"signature_evidence_id": "signature:override_event:ReceiveBeginPlay"
		},
		"statements": [
			{ "id": "stmt_print", "kind": "call", "target": "PrintString" }
		]
	})JSON");

	const TSharedPtr<FJsonObject> LogicSpec = ParseEventTaxonomyLogicSpec(*this, Json);

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, SemanticIR);
	FBlueprintHelperGraphFragmentDag Dag;
	FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, Dag);

	TestEqual(TEXT("event name metadata"), Dag.Metadata.FindRef(TEXT("event_name")), FString(TEXT("ReceiveBeginPlay")));
	TestEqual(TEXT("event taxonomy metadata"), Dag.Metadata.FindRef(TEXT("event_taxonomy")), FString(TEXT("override_event")));
	TestEqual(TEXT("source cluster metadata"), Dag.Metadata.FindRef(TEXT("source_cluster")), FString(TEXT("BlueprintSignature")));
	TestEqual(TEXT("signature evidence id metadata"), Dag.Metadata.FindRef(TEXT("signature_evidence_id")), FString(TEXT("signature:override_event:ReceiveBeginPlay")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyReplaceScopeResolverTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.Replace.ScopeResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyReplaceScopeResolverTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReplaceEntryResolveRequest CustomRequest;
	CustomRequest.Scope = EBlueprintHelperReplaceScope::CustomEventBody;
	CustomRequest.EntryName = TEXT("HandleOpened");
	TestTrue(TEXT("custom_event_body accepts custom event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_CustomEvent::StaticClass()));
	TestFalse(TEXT("custom_event_body rejects native event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_Event::StaticClass()));
	TestFalse(TEXT("custom_event_body rejects function entry class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(CustomRequest, UK2Node_FunctionEntry::StaticClass()));

	FBlueprintHelperReplaceEntryResolveRequest EventRequest;
	EventRequest.Scope = EBlueprintHelperReplaceScope::EventBody;
	EventRequest.EntryName = TEXT("ReceiveBeginPlay");
	TestTrue(TEXT("event_body accepts event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(EventRequest, UK2Node_Event::StaticClass()));
	TestFalse(TEXT("event_body rejects custom event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(EventRequest, UK2Node_CustomEvent::StaticClass()));

	FBlueprintHelperReplaceEntryResolveRequest FunctionRequest;
	FunctionRequest.Scope = EBlueprintHelperReplaceScope::FunctionBody;
	FunctionRequest.EntryName = TEXT("DoWork");
	TestTrue(TEXT("function_body accepts function entry class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(FunctionRequest, UK2Node_FunctionEntry::StaticClass()));
	TestFalse(TEXT("function_body rejects native event class"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(FunctionRequest, UK2Node_Event::StaticClass()));

	FBlueprintHelperReplaceEntryResolveRequest GraphRequest;
	GraphRequest.Scope = EBlueprintHelperReplaceScope::Graph;
	GraphRequest.EntryName = TEXT("ReceiveBeginPlay");
	TestFalse(TEXT("graph scope does not claim entry body nodes"),
		FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(GraphRequest, UK2Node_Event::StaticClass()));
	TestTrue(TEXT("graph scope preserves native event entry nodes"),
		FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(GraphRequest, UK2Node_Event::StaticClass()));
	TestTrue(TEXT("graph scope preserves custom event entry nodes"),
		FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(GraphRequest, UK2Node_CustomEvent::StaticClass()));
	TestFalse(TEXT("graph scope does not preserve function entry nodes"),
		FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(GraphRequest, UK2Node_FunctionEntry::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphEventTaxonomyReviewEvidencePreservesTaxonomyTest,
	"BlueprintHelper.GraphWrite.EventTaxonomy.ReviewEvidence.PreservesTaxonomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphEventTaxonomyReviewEvidencePreservesTaxonomyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphFragmentDag Dag;
	Dag.Schema = TEXT("BlueprintHelperGraphFragmentDag.v1");
	Dag.Metadata.Add(TEXT("review_scope_kind"), TEXT("event"));
	Dag.Metadata.Add(TEXT("event_name"), TEXT("ReceiveBeginPlay"));
	Dag.Metadata.Add(TEXT("event_taxonomy"), TEXT("override_event"));
	Dag.Metadata.Add(TEXT("source_cluster"), TEXT("BlueprintSignature"));

	FBlueprintHelperGraphFragmentRef Fragment;
	Fragment.FragmentId = TEXT("stmt_0");
	Fragment.SourceStatementId = TEXT("stmt_0");
	Fragment.Path = TEXT("$.statements[0]");
	Fragment.Kind = TEXT("call");
	Dag.Fragments.Add(Fragment);

	const FBlueprintHelperGraphFragmentEvidenceBundle Bundle =
		FBlueprintHelperGraphFragmentEvidenceBuilder::BuildFromDag(Dag);

	TestEqual(TEXT("one review scope"), Bundle.ReviewScopes.Num(), 1);
	if (Bundle.ReviewScopes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope = Bundle.ReviewScopes[0];
	TestEqual(TEXT("scope stays generic event"), Scope.ScopeKind, EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event);
	TestEqual(TEXT("event name"), Scope.EventName, FString(TEXT("ReceiveBeginPlay")));
	TestEqual(TEXT("taxonomy field"), Scope.EventTaxonomy, FString(TEXT("override_event")));
	TestEqual(TEXT("taxonomy metadata"), Scope.Metadata.FindRef(TEXT("event_taxonomy")), FString(TEXT("override_event")));
	return true;
}

#endif
