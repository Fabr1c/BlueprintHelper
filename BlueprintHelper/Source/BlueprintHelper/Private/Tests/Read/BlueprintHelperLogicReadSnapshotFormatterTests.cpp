#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadSnapshotFormatter_FormatsPureDto,
	"BlueprintHelper.Read.LogicSnapshotFormatter.FormatsPureDto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperLogicReadSnapshotFormatter_FormatsPureDto::RunTest(const FString& Parameters)
{
	FBlueprintHelperLogicReadSnapshot Snapshot;
	Snapshot.AssetPath = TEXT("/Game/BlueprintHelperRead/BP_Test");
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Scope = EBlueprintHelperLogicScope::TargetGraph;
	Snapshot.bExportSucceeded = false;

	FBlueprintHelperLogicReadSnapshotFormatter Formatter;
	TSharedPtr<FJsonObject> JsonPayload;
	FString Error;
	TestTrue(TEXT("logic_json payload builds from DTO"),
		Formatter.BuildFormattedPayload(TEXT("logic_json"), Snapshot, JsonPayload, Error));
	TestTrue(TEXT("logic_json payload valid"), JsonPayload.IsValid());
	TestEqual(TEXT("logic_json schema"),
		JsonPayload.IsValid() ? JsonPayload->GetStringField(TEXT("schema")) : FString(),
		FString(TEXT("LogicJson.v1")));

	TSharedPtr<FJsonObject> MdPayload;
	TestTrue(TEXT("logic_md payload builds from DTO"),
		Formatter.BuildFormattedPayload(TEXT("logic_md"), Snapshot, MdPayload, Error));
	TestTrue(TEXT("logic_md payload valid"), MdPayload.IsValid());
	TestEqual(TEXT("logic_md schema"),
		MdPayload.IsValid() ? MdPayload->GetStringField(TEXT("schema")) : FString(),
		FString(TEXT("LogicMd.v1")));

	TSharedPtr<FJsonObject> UnsupportedPayload;
	TestFalse(TEXT("unknown format fails"),
		Formatter.BuildFormattedPayload(TEXT("logic_xml"), Snapshot, UnsupportedPayload, Error));
	TestTrue(TEXT("unknown format reports error"), Error.Contains(TEXT("Unsupported read format")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadRequestSnapshotCache_IsRequestLocal,
	"BlueprintHelper.Read.LogicSnapshotCache.IsRequestLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperLogicReadRequestSnapshotCache_IsRequestLocal::RunTest(const FString& Parameters)
{
	FBlueprintHelperLogicReadSnapshotCacheKey Key;
	Key.AssetPath = TEXT("/Game/BlueprintHelperRead/BP_Test");
	Key.GraphName = TEXT("EventGraph");
	Key.Scope = TEXT("target_graph");
	Key.ReadDetail = TEXT("default");
	Key.SchemaVersion = TEXT("LogicReadSnapshot.v1");

	FBlueprintHelperLogicReadSnapshot Snapshot;
	Snapshot.AssetPath = Key.AssetPath;
	Snapshot.GraphName = Key.GraphName;

	FBlueprintHelperLogicReadRequestSnapshotCache Cache;
	FBlueprintHelperLogicReadSnapshot Found;
	TestFalse(TEXT("first lookup misses"), Cache.TryGet(Key, Found));
	Cache.Put(Key, Snapshot);
	TestTrue(TEXT("second lookup hits"), Cache.TryGet(Key, Found));
	TestEqual(TEXT("hit count"), Cache.GetHitCount(), 1);
	TestEqual(TEXT("miss count"), Cache.GetMissCount(), 1);
	Cache.Reset();
	TestFalse(TEXT("reset clears snapshot"), Cache.TryGet(Key, Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadCachePolicy_SeparatesPureDataFromUObjectState,
	"BlueprintHelper.Read.CachePolicy.SeparatesPureDataFromUObjectState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReadCachePolicy_SeparatesPureDataFromUObjectState::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("capability matrix can persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::ReadCapabilityMatrix));
	TestFalse(TEXT("asset snapshot cannot persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::AssetGraphSnapshot));
	TestTrue(TEXT("asset snapshot is request local only"),
		FBlueprintHelperReadCachePolicy::IsRequestLocalOnly(
			EBlueprintHelperReadCacheDataKind::AssetGraphSnapshot));
	TestFalse(TEXT("UObject pointer cannot persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::UObjectPointer));
	return true;
}

#endif
