#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperK2GraphEntryEvidence.h"

static FBlueprintHelperK2GraphEntryEvidence BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence()
{
	FBlueprintHelperK2GraphEntryEvidence Evidence;
	Evidence.AssetPath = TEXT("/Game/ShooterRange/Blueprints/BP_ShooterPlayerController");
	Evidence.GraphName = TEXT("EventGraph");
	Evidence.OperationKind = TEXT("replace_external_body");
	Evidence.EntryIdentity.Kind = EBlueprintHelperK2GraphEntryKind::Event;
	Evidence.EntryIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	Evidence.EntryIdentity.NodeGuid = TEXT("11112222333344445555666677778888");
	Evidence.EntryIdentity.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_Event");
	Evidence.EntryIdentity.StableName = TEXT("OnShooterFireStartedInput");
	Evidence.EntryIdentity.GraphName = Evidence.GraphName;
	Evidence.EntryIdentity.bValid = true;
	Evidence.BodyEntryAnchorJson = TEXT("{\"schema\":\"BlueprintHelper.ExternalGraphAnchor.v1\",\"node_guid\":\"11112222333344445555666677778888\"}");
	Evidence.BodyFingerprint = TEXT("body-fingerprint");
	Evidence.BeforeBodySnapshotJson = TEXT("{\"exists\":true,\"body_fingerprint\":\"before\"}");
	Evidence.AfterBodySnapshotJson = TEXT("{\"exists\":true,\"body_fingerprint\":\"after\"}");
	Evidence.GraphBodyBoundaryJson = TEXT("{\"runtime_adapter_id\":\"k2.external_body\"}");
	return Evidence;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2GraphEntryEvidenceProjectorTest,
	"BlueprintHelper.TaskRuntime.K2GraphEntryReviewEvidence.ProjectorBuildsAtomicTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2GraphEntryEvidenceProjectorTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewAtomicTarget Target;
	FString Error;
	const bool bProjected = FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(
		BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence(),
		3,
		0,
		Target,
		Error);

	TestTrue(TEXT("projector succeeds"), bProjected);
	TestTrue(TEXT("error remains empty"), Error.IsEmpty());
	TestEqual(TEXT("target kind"), Target.TargetKind, FString(TEXT("k2_graph_entry")));
	TestEqual(TEXT("target subkind"), Target.TargetSubKind, FString(TEXT("event")));
	TestEqual(TEXT("target key"),
		Target.TargetKey,
		FString(TEXT("k2_graph_entry:EventGraph:event:OnShooterFireStartedInput")));
	TestEqual(TEXT("anchor json"),
		Target.AnchorJson,
		FString(TEXT("{\"schema\":\"BlueprintHelper.ExternalGraphAnchor.v1\",\"node_guid\":\"11112222333344445555666677778888\"}")));
	TestEqual(TEXT("graph body boundary json"),
		Target.GraphBodyBoundaryJson,
		FString(TEXT("{\"runtime_adapter_id\":\"k2.external_body\"}")));
	TestEqual(TEXT("before snapshot"),
		Target.BeforeSnapshotJson,
		FString(TEXT("{\"exists\":true,\"body_fingerprint\":\"before\"}")));
	TestEqual(TEXT("after snapshot"),
		Target.AfterSnapshotJson,
		FString(TEXT("{\"exists\":true,\"body_fingerprint\":\"after\"}")));
	TestEqual(TEXT("after fingerprint"), Target.ReadbackFingerprintAfter, FString(TEXT("body-fingerprint")));
	TestEqual(TEXT("step index"), Target.TaskStepIndex, 3);
	TestEqual(TEXT("atomic index"), Target.AtomicIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2GraphEntryEvidenceRejectsIncompleteInputTest,
	"BlueprintHelper.TaskRuntime.K2GraphEntryReviewEvidence.RejectsIncompleteInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2GraphEntryEvidenceRejectsIncompleteInputTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewAtomicTarget Target;
	FString Error;

	FBlueprintHelperK2GraphEntryEvidence MissingAsset = BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence();
	MissingAsset.AssetPath.Reset();
	TestFalse(TEXT("missing asset path is rejected"),
		FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(MissingAsset, 0, 0, Target, Error));
	TestFalse(TEXT("missing asset path error"), Error.IsEmpty());

	FBlueprintHelperK2GraphEntryEvidence MissingGraph = BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence();
	MissingGraph.GraphName.Reset();
	TestFalse(TEXT("missing graph name is rejected"),
		FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(MissingGraph, 0, 0, Target, Error));
	TestFalse(TEXT("missing graph name error"), Error.IsEmpty());

	FBlueprintHelperK2GraphEntryEvidence InvalidIdentity = BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence();
	InvalidIdentity.EntryIdentity.bValid = false;
	TestFalse(TEXT("invalid identity is rejected"),
		FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(InvalidIdentity, 0, 0, Target, Error));
	TestFalse(TEXT("invalid identity error"), Error.IsEmpty());

	FBlueprintHelperK2GraphEntryEvidence MissingAnchor = BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence();
	MissingAnchor.BodyEntryAnchorJson.Reset();
	TestFalse(TEXT("missing anchor is rejected"),
		FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(MissingAnchor, 0, 0, Target, Error));
	TestFalse(TEXT("missing anchor error"), Error.IsEmpty());
	return true;
}

#endif
