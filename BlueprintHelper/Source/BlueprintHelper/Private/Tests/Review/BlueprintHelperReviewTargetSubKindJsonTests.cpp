#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.h"

class FBlueprintHelperReviewTargetSubKindJsonTestsLocalUtils
{
public:
	static FBlueprintHelperReviewRecord MakeRecordWithTargetSubKind()
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = TEXT("/Game/Test/BP_Signature.BP_Signature");
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
		Target.TargetKey = TEXT("signature:ComputeScore");
		Target.TargetKind = TEXT("signature");
		Target.TargetSubKind = TEXT("function");
		Target.SignatureRole = TEXT("dependency");
		Target.SignatureEvidenceId = TEXT("signature:function:ComputeScore");
		Target.DependencyOwnerStepId = TEXT("step_graph_body");
		Target.DependentStepId = TEXT("step_signature_function");
		Target.VisualGroupKey = TEXT("signature:function:ComputeScore");
		Target.DisplayLabel = TEXT("ComputeScore");
		Target.LatestEvidenceId = TEXT("evidence_signature");
		Target.SourceEvidenceIds.Add(TEXT("evidence_signature"));

		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = TEXT("change_signature");
		Change.AssetPath = Target.AssetPath;
		Change.LocationKey = Target.VisualGroupKey;
		Change.LatestEvidenceId = Target.LatestEvidenceId;
		Change.LatestEvidenceIds.Add(Target.LatestEvidenceId);
		Change.SourceEvidenceIds.Add(Target.LatestEvidenceId);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		Change.AtomicTargets.Add(Target);

		FBlueprintHelperReviewRecord Record;
		Record.ReviewRecordId = TEXT("record_signature");
		Record.ArchiveSessionId = TEXT("archive_signature");
		Record.AssetPath = Target.AssetPath;
		Record.SourceTaskRunIds.Add(TEXT("task_signature"));
		Record.VisibleChanges.Add(Change);
		return Record;
	}

	static TSharedRef<FJsonObject> MakeRecordJsonWithoutTargetSubKind()
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Signature.BP_Signature"));
		Target->SetStringField(TEXT("surface"), TEXT("my_blueprint"));
		Target->SetStringField(TEXT("target_key"), TEXT("signature:ComputeScore"));
		Target->SetStringField(TEXT("target_kind"), TEXT("signature"));
		Target->SetStringField(TEXT("visual_group_key"), TEXT("signature:ComputeScore"));
		Target->SetStringField(TEXT("status"), TEXT("pending"));

		TArray<TSharedPtr<FJsonValue>> Targets;
		Targets.Add(MakeShared<FJsonValueObject>(Target));

		TSharedRef<FJsonObject> Change = MakeShared<FJsonObject>();
		Change->SetStringField(TEXT("change_id"), TEXT("change_signature"));
		Change->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Signature.BP_Signature"));
		Change->SetStringField(TEXT("visual_group_key"), TEXT("signature:ComputeScore"));
		Change->SetStringField(TEXT("latest_evidence_id"), TEXT("evidence_signature"));
		Change->SetStringField(TEXT("change_kind"), TEXT("added"));
		Change->SetStringField(TEXT("status"), TEXT("pending"));
		Change->SetArrayField(TEXT("atomic_targets"), Targets);

		TArray<TSharedPtr<FJsonValue>> Changes;
		Changes.Add(MakeShared<FJsonValueObject>(Change));

		TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewRecord.v2"));
		Record->SetStringField(TEXT("review_record_id"), TEXT("record_signature"));
		Record->SetStringField(TEXT("archive_session_id"), TEXT("archive_signature"));
		Record->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Signature.BP_Signature"));
		Record->SetStringField(TEXT("status"), TEXT("pending"));
		Record->SetStringField(TEXT("storage_status"), TEXT("active"));
		Record->SetArrayField(TEXT("visible_changes"), Changes);
		return Record;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewTargetSubKindJsonRoundTripTest,
	"BlueprintHelper.Review.Json.TargetSubKindRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewTargetSubKindJsonRoundTripTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewTargetSubKindJsonTestsLocalUtils::MakeRecordWithTargetSubKind();
	const TSharedRef<FJsonObject> Json = FBlueprintHelperReviewStoreJsonUtils::ReviewRecordToJson(Record);

	const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
	TestTrue(TEXT("record writes visible changes"), Json->TryGetArrayField(TEXT("visible_changes"), Changes));
	const TSharedPtr<FJsonObject> ChangeJson =
		Changes && Changes->Num() == 1 ? (*Changes)[0]->AsObject() : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
	TestTrue(TEXT("change writes atomic targets"),
		ChangeJson.IsValid() && ChangeJson->TryGetArrayField(TEXT("atomic_targets"), Targets));
	const TSharedPtr<FJsonObject> TargetJson =
		Targets && Targets->Num() == 1 ? (*Targets)[0]->AsObject() : nullptr;
	FString WrittenSubKind;
	TestTrue(TEXT("target_subkind is written"),
		TargetJson.IsValid() && TargetJson->TryGetStringField(TEXT("target_subkind"), WrittenSubKind));
	TestEqual(TEXT("written target_subkind value"), WrittenSubKind, FString(TEXT("function")));
	FString WrittenSignatureEvidenceId;
	TestTrue(TEXT("signature_evidence_id is written"),
		TargetJson.IsValid() && TargetJson->TryGetStringField(TEXT("signature_evidence_id"), WrittenSignatureEvidenceId));
	TestEqual(TEXT("written signature_evidence_id value"),
		WrittenSignatureEvidenceId,
		FString(TEXT("signature:function:ComputeScore")));

	FBlueprintHelperReviewRecord Loaded;
	TestTrue(TEXT("record reads from json"),
		FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, Loaded));
	TestEqual(TEXT("one change reads back"), Loaded.VisibleChanges.Num(), 1);
	const FBlueprintHelperReviewVisibleChange& LoadedChange =
		Loaded.VisibleChanges.Num() == 1 ? Loaded.VisibleChanges[0] : FBlueprintHelperReviewVisibleChange();
	TestEqual(TEXT("one target reads back"), LoadedChange.AtomicTargets.Num(), 1);
	if (LoadedChange.AtomicTargets.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("target_subkind roundtrips"),
		LoadedChange.AtomicTargets[0].TargetSubKind,
		FString(TEXT("function")));
	TestEqual(TEXT("signature_role roundtrips"),
		LoadedChange.AtomicTargets[0].SignatureRole,
		FString(TEXT("dependency")));
	TestEqual(TEXT("signature_evidence_id roundtrips"),
		LoadedChange.AtomicTargets[0].SignatureEvidenceId,
		FString(TEXT("signature:function:ComputeScore")));
	TestEqual(TEXT("dependency_owner_step_id roundtrips"),
		LoadedChange.AtomicTargets[0].DependencyOwnerStepId,
		FString(TEXT("step_graph_body")));
	TestEqual(TEXT("dependent_step_id roundtrips"),
		LoadedChange.AtomicTargets[0].DependentStepId,
		FString(TEXT("step_signature_function")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewTargetSubKindJsonAbsentReadsEmptyTest,
	"BlueprintHelper.Review.Json.TargetSubKindAbsentReadsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewTargetSubKindJsonAbsentReadsEmptyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewRecord Loaded;
	TestTrue(TEXT("record without target_subkind reads"),
		FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(
			FBlueprintHelperReviewTargetSubKindJsonTestsLocalUtils::MakeRecordJsonWithoutTargetSubKind(),
			Loaded));
	TestEqual(TEXT("one change reads"), Loaded.VisibleChanges.Num(), 1);
	const FBlueprintHelperReviewVisibleChange& LoadedChange =
		Loaded.VisibleChanges.Num() == 1 ? Loaded.VisibleChanges[0] : FBlueprintHelperReviewVisibleChange();
	TestEqual(TEXT("one target reads"), LoadedChange.AtomicTargets.Num(), 1);
	if (LoadedChange.AtomicTargets.Num() != 1)
	{
		return false;
	}
	TestTrue(TEXT("absent target_subkind stays empty"),
		LoadedChange.AtomicTargets[0].TargetSubKind.IsEmpty());
	TestTrue(TEXT("absent signature metadata stays empty"),
		LoadedChange.AtomicTargets[0].SignatureEvidenceId.IsEmpty() &&
		LoadedChange.AtomicTargets[0].DependencyOwnerStepId.IsEmpty() &&
		LoadedChange.AtomicTargets[0].DependentStepId.IsEmpty());
	return true;
}

#endif
