#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeEventDelegateActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeEventDelegateActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperEventDelegateAction/%s"),
		*MakeEventDelegateActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeEventDelegateActionTestObjectName(TEXT("BP_EventDelegateAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperEventDelegateActionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetEventDelegateActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeEventDelegateActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeEventDelegateActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("event_delegate_projected_context");
	Request.SemanticConstraintsHash = TEXT("event_delegate_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.MaxCandidates = 8;
	return Request;
}

static bool HasCustomEventSpawnerCandidate(const FBlueprintHelperActionResolutionResult& Result)
{
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Result.CandidateActions)
	{
		if (Candidate.MatchReason.Contains(TEXT("ue_custom_event_node_spawner"))
			|| Candidate.NodeClassPath.Contains(TEXT("K2Node_CustomEvent")))
		{
			return true;
		}
	}
	return false;
}

static bool AssertMissingEvidenceDiagnostic(
	FAutomationTestBase& Test,
	const FString& Label,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedDetail)
{
	bool bPassed = true;
	bPassed &= Test.TestNotEqual(*FString::Printf(TEXT("%s not resolved"), *Label), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s error code"), *Label), Result.ErrorCode, FString(TEXT("missing_required_evidence")));
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s message names missing evidence"), *Label), Result.Message.Contains(ExpectedDetail));
	return bPassed;
}

static bool AssertUnsupportedCompleteDelegateBoundary(
	FAutomationTestBase& Test,
	const FString& Label,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedSemanticName,
	const FString& ExpectedDelegateName)
{
	bool bPassed = true;
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s status"), *Label), Result.Status, EBlueprintHelperActionResolutionStatus::UnsupportedIntent);
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s error code"), *Label), Result.ErrorCode, FString(TEXT("unsupported_intent")));
	bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s no selected spawner"), *Label), Result.SelectedSpawner.IsValid());
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s no selected stable id"), *Label), Result.SelectedStableId.IsEmpty());
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s no candidate success"), *Label), Result.CandidateActions.Num(), 0);
	const FString ExpectedMessage = FString::Printf(
		TEXT("%s delegate spawner boundary is not complete for delegate '%s': missing safe UE spawner-family routing for projected component/binding object plus delegate signature evidence."),
		*ExpectedSemanticName,
		*ExpectedDelegateName);
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s exact unsupported boundary"), *Label), Result.Message, ExpectedMessage);
	return bPassed;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateCustomEventPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.P5.CustomEventPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateCustomEventPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FString EventName = TEXT("ApplyConfigRequested");
	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Event, EventName));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestTrue(TEXT("stable id contains event name"), Result.SelectedStableId.Contains(EventName));
	TestTrue(TEXT("candidate records UBlueprintEventNodeSpawner evidence"), HasCustomEventSpawnerCandidate(Result));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateMissingEventNameTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.P5.CustomEventMissingEventName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateMissingEventNameTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Event));

	AssertMissingEvidenceDiagnostic(*this, TEXT("custom event missing event name"), Result, TEXT("event_name_missing"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateComponentBoundMissingEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.P5.ComponentBoundMissingEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateComponentBoundMissingEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest MissingComponent =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, TEXT("OnComponentBeginOverlap"));
	MissingComponent.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnComponentBeginOverlap"));
	MissingComponent.ContextEvidence.Add(TEXT("delegate_signature"), TEXT("FComponentBeginOverlapSignature"));
	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("component bound event missing component"),
		FBlueprintHelperActionResolutionCore::Resolve(MissingComponent),
		TEXT("component_missing"));

	FBlueprintHelperActionResolutionRequest MissingSignature =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, TEXT("OnComponentBeginOverlap"));
	MissingSignature.ContextEvidence.Add(TEXT("component_path"), TEXT("DoorTrigger"));
	MissingSignature.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnComponentBeginOverlap"));
	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("component bound event missing signature"),
		FBlueprintHelperActionResolutionCore::Resolve(MissingSignature),
		TEXT("delegate_signature_missing"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindMissingEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.P5.BindMissingEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindMissingEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest MissingBindingObject =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Bind, TEXT("OnConfigChanged"));
	MissingBindingObject.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnConfigChanged"));
	MissingBindingObject.ContextEvidence.Add(TEXT("delegate_signature"), TEXT("FConfigChangedSignature"));
	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("bind missing binding object"),
		FBlueprintHelperActionResolutionCore::Resolve(MissingBindingObject),
		TEXT("binding_object_missing"));

	FBlueprintHelperActionResolutionRequest MissingSignature =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Bind, TEXT("OnConfigChanged"));
	MissingSignature.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("ConfigSource"));
	MissingSignature.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnConfigChanged"));
	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("bind missing signature"),
		FBlueprintHelperActionResolutionCore::Resolve(MissingSignature),
		TEXT("delegate_signature_missing"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateCompleteBoundaryUnsupportedTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.P5.CompleteBoundaryUnsupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateCompleteBoundaryUnsupportedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest ComponentBound =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, TEXT("OnComponentBeginOverlap"));
	ComponentBound.ContextEvidence.Add(TEXT("component_path"), TEXT("DoorTrigger"));
	ComponentBound.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnComponentBeginOverlap"));
	ComponentBound.ContextEvidence.Add(TEXT("delegate_signature"), TEXT("FComponentBeginOverlapSignature"));
	ComponentBound.ContextEvidence.Add(TEXT("target_graph"), Graph ? Graph->GetName() : TEXT("EventGraph"));
	AssertUnsupportedCompleteDelegateBoundary(
		*this,
		TEXT("complete component-bound event"),
		FBlueprintHelperActionResolutionCore::Resolve(ComponentBound),
		TEXT("component_bound_event"),
		TEXT("OnComponentBeginOverlap"));

	FBlueprintHelperActionResolutionRequest Bind =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Bind, TEXT("OnConfigChanged"));
	Bind.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("ConfigSource"));
	Bind.ContextEvidence.Add(TEXT("delegate_name"), TEXT("OnConfigChanged"));
	Bind.ContextEvidence.Add(TEXT("delegate_signature"), TEXT("FConfigChangedSignature"));
	Bind.ContextEvidence.Add(TEXT("target_graph"), Graph ? Graph->GetName() : TEXT("EventGraph"));
	AssertUnsupportedCompleteDelegateBoundary(
		*this,
		TEXT("complete delegate bind"),
		FBlueprintHelperActionResolutionCore::Resolve(Bind),
		TEXT("bind"),
		TEXT("OnConfigChanged"));

	return true;
}

#endif
