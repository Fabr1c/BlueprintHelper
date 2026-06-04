#include "Runtime/TaskRuntime/BlueprintHelperTaskPreviewStore.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteCandidateArtifactStore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAutoSearchCandidate_ReturnsCandidatesForAmbiguousCall,
	"BlueprintHelper.TaskRuntime.AutoSearchCandidate.ReturnsCandidatesForAmbiguousCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeAutoSearchCandidate_ReturnsCandidatesForAmbiguousCall::RunTest(const FString& Parameters)
{
	FBlueprintHelperCallFunctionResolveResult Result;
	Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
	Result.ErrorCode = TEXT("ambiguous_function_call");

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = TEXT("/Script/Engine.KismetSystemLibrary:PrintString");
	Candidate.DisplayName = TEXT("Print String");
	Candidate.OwnerClassPath = TEXT("/Script/Engine.KismetSystemLibrary");
	Candidate.MatchReason = TEXT("display exact");
	Result.CandidateFunctions.Add(Candidate);

	TestEqual(
		TEXT("candidate stable id"),
		Result.CandidateFunctions[0].StableId,
		FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
	TestEqual(TEXT("candidate match reason"), Result.CandidateFunctions[0].MatchReason, FString(TEXT("display exact")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAutoSearchCandidate_OutputIsShortAndSafe,
	"BlueprintHelper.TaskRuntime.AutoSearchCandidate.OutputIsShortAndSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeAutoSearchCandidate_OutputIsShortAndSafe::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Candidate = MakeShared<FJsonObject>();
	Candidate->SetStringField(TEXT("candidate_id"), TEXT("preview:gw_01:s_print:001"));
	Candidate->SetStringField(TEXT("suggested_kind"), TEXT("call"));
	Candidate->SetStringField(TEXT("display_name"), TEXT("Print String"));
	Candidate->SetStringField(TEXT("owner_short"), TEXT("KismetSystemLibrary"));
	Candidate->SetStringField(TEXT("node_class"), TEXT("K2Node_CallFunction"));
	Candidate->SetStringField(TEXT("match_reason"), TEXT("target text + graph context compatible"));

	TestFalse(TEXT("no stable id exposed in short candidate"), Candidate->HasField(TEXT("stable_id")));
	TestFalse(TEXT("no spawner signature exposed in short candidate"), Candidate->HasField(TEXT("spawner_signature")));
	TestFalse(TEXT("no UObject type exposed in short candidate"), Candidate->HasField(TEXT("UBlueprintNodeSpawner")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAutoSearchCandidate_PreviewStorePreservesArtifact,
	"BlueprintHelper.TaskRuntime.AutoSearchCandidate.PreviewStorePreservesArtifact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeAutoSearchCandidate_PreviewStorePreservesArtifact::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPreviewStore Store;
	FBlueprintHelperTaskPreviewStoreCreateRequest Request;
	Request.TaskSpecHash = TEXT("task_hash");
	Request.TaskPlanHash = TEXT("plan_hash");
	Request.ExecutionPolicyHash = TEXT("policy_hash");
	Request.AssetStateHash = TEXT("asset_hash");
	Request.ActionContextRevisionManifestHash = TEXT("ctx_hash");
	Request.GraphWriteCandidateArtifactHash = TEXT("artifact_hash");
	Request.GraphWriteCandidateArtifactJson = MakeShared<FJsonObject>();
	Request.GraphWriteCandidateArtifactJson->SetStringField(TEXT("snapshot_generation"), TEXT("gen_1"));
	Request.bPassed = false;

	const FString Token = Store.Store(Request);
	Request.GraphWriteCandidateArtifactJson->SetStringField(TEXT("snapshot_generation"), TEXT("mutated_after_store"));

	FBlueprintHelperTaskPreviewStoreResolveResult Resolved = Store.Resolve(Token, TEXT("task_hash"));
	TestTrue(TEXT("resolve ok"), Resolved.bOk);
	TestEqual(TEXT("artifact hash preserved"), Resolved.GraphWriteCandidateArtifactHash, FString(TEXT("artifact_hash")));
	TestTrue(TEXT("artifact exists"), Resolved.GraphWriteCandidateArtifactJson.IsValid());
	TestEqual(
		TEXT("snapshot generation preserved"),
		Resolved.GraphWriteCandidateArtifactJson->GetStringField(TEXT("snapshot_generation")),
		FString(TEXT("gen_1")));

	Resolved.GraphWriteCandidateArtifactJson->SetStringField(TEXT("snapshot_generation"), TEXT("mutated_after_resolve"));
	const FBlueprintHelperTaskPreviewStoreResolveResult ResolvedAgain = Store.Resolve(Token, TEXT("task_hash"));
	TestTrue(TEXT("resolve again ok"), ResolvedAgain.bOk);
	TestEqual(
		TEXT("resolved artifact is cloned"),
		ResolvedAgain.GraphWriteCandidateArtifactJson->GetStringField(TEXT("snapshot_generation")),
		FString(TEXT("gen_1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAutoSearchCandidate_CandidateArtifactStoreUsesFullKeyAndClones,
	"BlueprintHelper.TaskRuntime.AutoSearchCandidate.CandidateArtifactStoreUsesFullKeyAndClones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeAutoSearchCandidate_CandidateArtifactStoreUsesFullKeyAndClones::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteCandidateArtifactStore Store;

	FBlueprintHelperGraphWriteCandidateArtifactRecord Artifact;
	Artifact.PreviewToken = TEXT("preview_token");
	Artifact.StatementId = TEXT("statement_a");
	Artifact.CandidateId = TEXT("preview:gw_01:statement_a:001");
	Artifact.CandidateHash = TEXT("candidate_hash");
	Artifact.EvidenceJson = MakeShared<FJsonObject>();
	Artifact.EvidenceJson->SetStringField(TEXT("stable_id"), TEXT("stable_a"));
	Store.Store(Artifact);

	Artifact.EvidenceJson->SetStringField(TEXT("stable_id"), TEXT("mutated_after_store"));

	FBlueprintHelperGraphWriteCandidateArtifactRecord Resolved;
	TestTrue(
		TEXT("full key resolves"),
		Store.TryResolve(TEXT("preview_token"), TEXT("statement_a"), TEXT("preview:gw_01:statement_a:001"), Resolved));
	TestEqual(TEXT("hash preserved"), Resolved.CandidateHash, FString(TEXT("candidate_hash")));
	TestTrue(TEXT("evidence exists"), Resolved.EvidenceJson.IsValid());
	TestEqual(TEXT("stored evidence cloned"), Resolved.EvidenceJson->GetStringField(TEXT("stable_id")), FString(TEXT("stable_a")));

	Resolved.EvidenceJson->SetStringField(TEXT("stable_id"), TEXT("mutated_after_resolve"));
	FBlueprintHelperGraphWriteCandidateArtifactRecord ResolvedAgain;
	TestTrue(
		TEXT("full key resolves again"),
		Store.TryResolve(TEXT("preview_token"), TEXT("statement_a"), TEXT("preview:gw_01:statement_a:001"), ResolvedAgain));
	TestEqual(TEXT("resolved evidence cloned"), ResolvedAgain.EvidenceJson->GetStringField(TEXT("stable_id")), FString(TEXT("stable_a")));

	FBlueprintHelperGraphWriteCandidateArtifactRecord NotFound;
	TestFalse(
		TEXT("preview token mismatch misses"),
		Store.TryResolve(TEXT("other_preview"), TEXT("statement_a"), TEXT("preview:gw_01:statement_a:001"), NotFound));
	TestFalse(
		TEXT("statement mismatch misses"),
		Store.TryResolve(TEXT("preview_token"), TEXT("statement_b"), TEXT("preview:gw_01:statement_a:001"), NotFound));
	TestFalse(
		TEXT("candidate mismatch misses"),
		Store.TryResolve(TEXT("preview_token"), TEXT("statement_a"), TEXT("preview:gw_01:statement_a:002"), NotFound));
	return true;
}

#endif
