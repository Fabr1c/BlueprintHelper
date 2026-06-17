#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"
#include "Systems/Debug/BlueprintHelperDebugCaseProjectionRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"

class FBlueprintHelperDebugCaseExportProjectionTestsLocalUtils
{
public:
	static FBlueprintHelperDebugCase MakeDebugCase()
	{
		FBlueprintHelperDebugCase DebugCase;
		DebugCase.DebugCaseId = TEXT("dbg_projection_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		DebugCase.CreatedAt = TEXT("2026-06-15T00:00:00Z");
		DebugCase.UpdatedAt = DebugCase.CreatedAt;
		DebugCase.Source = TEXT("projection_test");
		DebugCase.Severity = EBlueprintHelperDebugSeverity::Error;
		DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
		DebugCase.Operation = TEXT("execute_task_plan");
		DebugCase.Stage = TEXT("execute");
		DebugCase.TraceIds.Add(TEXT("trace_projection"));
		DebugCase.TaskRunId = TEXT("task_projection");
		DebugCase.AssetPaths.Add(TEXT("/Game/BP_DebugProjection"));
		DebugCase.ReviewRecordIds.Add(TEXT("review_projection"));
		DebugCase.Error.Code = TEXT("projection_error");
		DebugCase.Error.Message = TEXT("Projection error.");
		DebugCase.FragmentArtifacts.FragmentCount = 2;
		DebugCase.FragmentArtifacts.EvidenceFragmentCount = 1;
		DebugCase.FragmentArtifacts.FragmentSignature = TEXT("fragment_signature");

		FBlueprintHelperDebugEvidenceLink Evidence;
		Evidence.EvidenceId = TEXT("evidence_projection");
		Evidence.Role = TEXT("task_source");
		Evidence.Source = TEXT("task_runtime");
		Evidence.Summary = TEXT("Projection evidence.");
		DebugCase.EvidenceLinks.Add(Evidence);

		FBlueprintHelperDebugEvent Event;
		Event.DebugEventId = TEXT("ev_projection");
		Event.DebugCaseId = DebugCase.DebugCaseId;
		Event.CreatedAt = DebugCase.CreatedAt;
		Event.SourceLayer = TEXT("task_runtime");
		Event.Source = DebugCase.Source;
		Event.Operation = DebugCase.Operation;
		Event.Stage = DebugCase.Stage;
		Event.Error = DebugCase.Error;
		DebugCase.Events.Add(Event);
		return DebugCase;
	}

	static FString ExpectedDebugCasePath(const FString& DebugCaseId)
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Debug")
			/ TEXT("Cases")
			/ FString::Printf(TEXT("%s.json"), *DebugCaseId);
	}

	static void CleanupDebugCaseFile(const FString& DebugCaseId)
	{
		IFileManager::Get().Delete(*ExpectedDebugCasePath(DebugCaseId), false, true);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCaseExportProjectionBuildsArtifactModelsTest,
	"BlueprintHelper.DebugCaseExport.ProjectionBuildsArtifactModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCaseExportProjectionBuildsArtifactModelsTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperDebugCase DebugCase =
		FBlueprintHelperDebugCaseExportProjectionTestsLocalUtils::MakeDebugCase();
	const FBlueprintHelperDebugCaseProjectionRegistry Registry =
		FBlueprintHelperDebugCaseProjectionRegistry::BuildDefault();
	FBlueprintHelperDebugCaseProjectionResult Result;
	FString Error;
	TestTrue(TEXT("projection registry builds artifacts"),
		Registry.BuildProjection(DebugCase, FBlueprintHelperDebugCaseProjectionContext(), Result, &Error));
	if (!Error.IsEmpty())
	{
		AddError(FString::Printf(TEXT("Unexpected projection error: %s"), *Error));
	}

	int32 JsonArtifactCount = 0;
	int32 MarkdownArtifactCount = 0;
	TSet<FString> RelativePaths;
	for (const FBlueprintHelperDebugCaseArtifactModel& Artifact : Result.Artifacts)
	{
		if (Artifact.Json.IsValid())
		{
			++JsonArtifactCount;
		}
		if (Artifact.Role == EBlueprintHelperDebugCaseArtifactRole::SummaryMarkdown)
		{
			++MarkdownArtifactCount;
		}
		RelativePaths.Add(Artifact.RelativePath);
	}

	TestEqual(TEXT("one evidence, one review id, one fragment summary, one debug case summary produce four JSON artifact models"),
		JsonArtifactCount,
		4);
	TestEqual(TEXT("projection adds one markdown summary artifact"), MarkdownArtifactCount, 1);
	TestTrue(TEXT("debug case summary artifact exists"),
		RelativePaths.Contains(TEXT("artifacts/debug_case.summary.json")));
	TestTrue(TEXT("evidence summary artifact exists"),
		RelativePaths.Contains(TEXT("artifacts/evidence/evidence_projection.summary.json")));
	TestTrue(TEXT("fragment summary artifact exists"),
		RelativePaths.Contains(TEXT("artifacts/graph_fragment.summary.json")));
	TestTrue(TEXT("review id summary artifact exists when ReviewStore is unavailable"),
		RelativePaths.Contains(TEXT("artifacts/review/review_record_ids.summary.json")));
	TestTrue(TEXT("review store unavailable is recorded as skipped metadata"),
		Result.SkippedArtifacts.ContainsByPredicate([](const FBlueprintHelperDebugSkippedArtifact& Skipped)
		{
			return Skipped.Artifact == TEXT("review_summaries");
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCaseExportStoreDoesNotBuildArtifactsTest,
	"BlueprintHelper.DebugCaseExport.StoreDoesNotBuildArtifacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCaseExportStoreDoesNotBuildArtifactsTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperDebugCase DebugCase =
		FBlueprintHelperDebugCaseExportProjectionTestsLocalUtils::MakeDebugCase();
	FBlueprintHelperDebugCaseExportProjectionTestsLocalUtils::CleanupDebugCaseFile(DebugCase.DebugCaseId);

	FBlueprintHelperDebugCaseStoreService Store;
	FString Error;
	TestTrue(TEXT("store saves debug case"), Store.SaveCase(DebugCase, &Error));

	FBlueprintHelperDebugCaseSummary Summary;
	TestTrue(TEXT("store query returns summary"), Store.QueryCaseSummary(DebugCase.DebugCaseId, Summary, &Error));
	const TSharedRef<FJsonObject> SummaryJson = Summary.ToJson();
	TestFalse(TEXT("store summary does not expose full events"), SummaryJson->HasField(TEXT("events")));
	TestFalse(TEXT("store summary does not expose manifest contents"), SummaryJson->HasField(TEXT("contents")));
	TestFalse(TEXT("store summary does not expose bundle summary ref"), SummaryJson->HasField(TEXT("summary_ref")));
	TestFalse(TEXT("store summary does not expose review summary refs"), SummaryJson->HasField(TEXT("review_summary_refs")));

	FBlueprintHelperDebugCaseExportProjectionTestsLocalUtils::CleanupDebugCaseFile(DebugCase.DebugCaseId);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
