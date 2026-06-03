#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskPreviewStore.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeContextRevisionManifest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeContextRevisionManifestStableJsonTest,
	"BlueprintHelper.TaskRuntime.ContextRevision.ManifestStableJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeContextRevisionManifestStableJsonTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeContextRevisionEntry Entry;
	Entry.AssetPath = TEXT("/Game/Blueprints/BP_Test");
	Entry.GraphName = TEXT("EventGraph");
	Entry.bGraphExists = true;
	Entry.BlueprintRevision = 11;
	Entry.GraphRevision = 22;

	FBlueprintHelperTaskRuntimeContextRevisionManifest Manifest;
	Manifest.Entries.Add(Entry);
	Manifest.RecomputeHash();

	const TSharedRef<FJsonObject> Json = Manifest.ToJson();
	FString ManifestHash;
	TestTrue(TEXT("manifest hash exists"), Json->TryGetStringField(TEXT("manifest_hash"), ManifestHash));
	TestEqual(TEXT("json manifest hash is stable hash"), ManifestHash, Manifest.ManifestHash);

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	TestTrue(TEXT("entries exists"), Json->TryGetArrayField(TEXT("entries"), Entries) && Entries && Entries->Num() == 1);

	const FBlueprintHelperTaskRuntimeContextRevisionManifest RoundTrip =
		FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromJson(Json);
	TestEqual(TEXT("round-trip manifest hash"), RoundTrip.ManifestHash, Manifest.ManifestHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeContextRevisionManifestMismatchTest,
	"BlueprintHelper.TaskRuntime.ContextRevision.ManifestMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeContextRevisionManifestMismatchTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeContextRevisionEntry ExpectedEntry;
	ExpectedEntry.AssetPath = TEXT("/Game/Blueprints/BP_Test");
	ExpectedEntry.GraphName = TEXT("EventGraph");
	ExpectedEntry.bGraphExists = true;
	ExpectedEntry.BlueprintRevision = 11;
	ExpectedEntry.GraphRevision = 22;

	FBlueprintHelperTaskRuntimeContextRevisionEntry CurrentEntry = ExpectedEntry;
	CurrentEntry.GraphRevision = 23;

	FBlueprintHelperTaskRuntimeContextRevisionManifest Expected;
	Expected.Entries.Add(ExpectedEntry);
	Expected.RecomputeHash();

	FBlueprintHelperTaskRuntimeContextRevisionManifest Current;
	Current.Entries.Add(CurrentEntry);
	Current.RecomputeHash();

	FBlueprintHelperTaskRuntimeContextRevisionMismatch Mismatch;
	const bool bMatches = FBlueprintHelperTaskRuntimeContextRevisionManifest::Compare(Expected, Current, Mismatch);

	TestFalse(TEXT("changed graph revision is mismatch"), bMatches);
	TestEqual(TEXT("mismatch code"), Mismatch.Code, FString(TEXT("context_stale")));
	TestEqual(TEXT("detail code"), Mismatch.DetailCode, FString(TEXT("action_context_stale")));
	TestTrue(TEXT("expected manifest json exists"), Mismatch.Expected.IsValid());
	TestTrue(TEXT("current manifest json exists"), Mismatch.Current.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePreviewStoreContextManifestTest,
	"BlueprintHelper.TaskRuntime.ContextRevision.PreviewStorePersistsManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimePreviewStoreContextManifestTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPreviewStore Store;
	FBlueprintHelperTaskPreviewStoreCreateRequest Request;
	Request.TaskSpecHash = TEXT("spec");
	Request.TaskPlanHash = TEXT("plan");
	Request.ExecutionPolicyHash = TEXT("policy");
	Request.AssetStateHash = TEXT("asset");
	Request.ActionContextRevisionManifestHash = TEXT("ctx_hash");
	Request.ActionContextRevisionManifestJson = MakeShared<FJsonObject>();
	Request.ActionContextRevisionManifestJson->SetStringField(TEXT("manifest_hash"), TEXT("ctx_hash"));
	Request.bPassed = true;

	const FString Token = Store.Store(Request);
	const FBlueprintHelperTaskPreviewStoreResolveResult Resolved = Store.Resolve(Token, TEXT("spec"));

	TestTrue(TEXT("preview token resolves"), Resolved.bOk);
	TestEqual(TEXT("manifest hash preserved"), Resolved.ActionContextRevisionManifestHash, FString(TEXT("ctx_hash")));
	TestTrue(TEXT("manifest json preserved"), Resolved.ActionContextRevisionManifestJson.IsValid());
	return true;
}

#endif
