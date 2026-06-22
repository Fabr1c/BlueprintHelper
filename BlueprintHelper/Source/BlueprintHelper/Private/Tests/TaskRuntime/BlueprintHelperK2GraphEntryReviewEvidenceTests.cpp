#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperK2GraphEntryEvidence.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TSharedPtr<FJsonObject> BlueprintHelperK2GraphEntryReviewEvidenceParseJsonObject(const FString& JsonText)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	return FJsonSerializer::Deserialize(Reader, JsonObject) ? JsonObject : nullptr;
}

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
	Evidence.BeforeBodyFingerprint = TEXT("before-body-fingerprint");
	Evidence.AfterBodyFingerprint = TEXT("after-body-fingerprint");
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
	const TSharedPtr<FJsonObject> AnchorJson =
		BlueprintHelperK2GraphEntryReviewEvidenceParseJsonObject(Target.AnchorJson);
	TestTrue(TEXT("anchor json parses"), AnchorJson.IsValid());
	if (AnchorJson.IsValid())
	{
		TestEqual(TEXT("anchor schema"),
			AnchorJson->GetStringField(TEXT("schema")),
			FString(TEXT("BlueprintHelper.ExternalGraphAnchor.v1")));
		TestEqual(TEXT("anchor node guid"),
			AnchorJson->GetStringField(TEXT("node_guid")),
			FString(TEXT("11112222333344445555666677778888")));
	}
	const TSharedPtr<FJsonObject> BoundaryJson =
		BlueprintHelperK2GraphEntryReviewEvidenceParseJsonObject(Target.GraphBodyBoundaryJson);
	TestTrue(TEXT("graph body boundary json parses"), BoundaryJson.IsValid());
	if (BoundaryJson.IsValid())
	{
		TestEqual(TEXT("boundary runtime adapter"),
			BoundaryJson->GetStringField(TEXT("runtime_adapter_id")),
			FString(TEXT("k2.external_body")));
	}
	const TSharedPtr<FJsonObject> BeforeSnapshotJson =
		BlueprintHelperK2GraphEntryReviewEvidenceParseJsonObject(Target.BeforeSnapshotJson);
	TestTrue(TEXT("before snapshot json parses"), BeforeSnapshotJson.IsValid());
	if (BeforeSnapshotJson.IsValid())
	{
		TestTrue(TEXT("before snapshot exists"), BeforeSnapshotJson->GetBoolField(TEXT("exists")));
		TestEqual(TEXT("before snapshot fingerprint"),
			BeforeSnapshotJson->GetStringField(TEXT("body_fingerprint")),
			FString(TEXT("before")));
	}
	const TSharedPtr<FJsonObject> AfterSnapshotJson =
		BlueprintHelperK2GraphEntryReviewEvidenceParseJsonObject(Target.AfterSnapshotJson);
	TestTrue(TEXT("after snapshot json parses"), AfterSnapshotJson.IsValid());
	if (AfterSnapshotJson.IsValid())
	{
		TestTrue(TEXT("after snapshot exists"), AfterSnapshotJson->GetBoolField(TEXT("exists")));
		TestEqual(TEXT("after snapshot fingerprint"),
			AfterSnapshotJson->GetStringField(TEXT("body_fingerprint")),
			FString(TEXT("after")));
	}
	TestEqual(TEXT("after fingerprint"), Target.ReadbackFingerprintAfter, FString(TEXT("after-body-fingerprint")));
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

	FBlueprintHelperK2GraphEntryEvidence MissingAfterFingerprint = BlueprintHelperK2GraphEntryReviewEvidenceMakeValidEvidence();
	MissingAfterFingerprint.AfterBodyFingerprint.Reset();
	TestFalse(TEXT("missing after body fingerprint is rejected"),
		FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(MissingAfterFingerprint, 0, 0, Target, Error));
	TestEqual(TEXT("missing after body fingerprint error"),
		Error,
		FString(TEXT("k2_graph_entry_evidence_missing_after_body_fingerprint")));
	return true;
}

#endif
