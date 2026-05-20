#include "Runtime/TaskRuntime/BlueprintHelperGraphWritePlanCache.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperGraphWritePlanCacheTestsLocalUtils
{
public:
	static FBlueprintHelperGraphWritePlanCacheKey MakeKey()
	{
		FBlueprintHelperGraphWritePlanCacheKey Key;
		Key.PayloadHash = TEXT("payload_hash");
		Key.GraphSchemaHash = TEXT("schema_hash");
		Key.AssetStateHash = TEXT("asset_state_hash");
		return Key;
	}

	static FBlueprintHelperGraphWritePlanCacheEntry MakeEntry()
	{
		FBlueprintHelperGraphWritePlanCacheEntry Entry;
		Entry.NormalizedGraphName = TEXT("eventgraph");
		Entry.OrderedOpSummary = {TEXT("0:call_function")};
		Entry.NodeIdToPlannedNodeKind.Add(TEXT("node_001"), TEXT("call_function"));
		Entry.PinAliasMap.Add(TEXT("node_001.execute"), TEXT("Execute"));
		Entry.ResolvedCallFunctionStableIds = {TEXT("/Script/Engine.KismetSystemLibrary:PrintString")};
		return Entry;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePlanCache_HitRequiresAllKeyParts,
	"BlueprintHelper.TaskRuntime.GraphWritePlanCache.HitRequiresAllKeyParts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWritePlanCache_HitRequiresAllKeyParts::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWritePlanCache Cache(FBlueprintHelperTaskRuntimeCacheConfig::Default());
	const FDateTime Now = FDateTime::UtcNow();
	const FBlueprintHelperGraphWritePlanCacheKey Key =
		FBlueprintHelperGraphWritePlanCacheTestsLocalUtils::MakeKey();
	Cache.Store(Key, FBlueprintHelperGraphWritePlanCacheTestsLocalUtils::MakeEntry(), Now);

	FBlueprintHelperGraphWritePlanCacheEntry Found;
	TestTrue(TEXT("matching key hits"), Cache.TryGet(Key, Now, Found));

	FBlueprintHelperGraphWritePlanCacheKey ChangedAsset = Key;
	ChangedAsset.AssetStateHash = TEXT("asset_state_hash_2");
	TestFalse(TEXT("asset state mismatch misses"), Cache.TryGet(ChangedAsset, Now, Found));

	FBlueprintHelperGraphWritePlanCacheKey ChangedSchema = Key;
	ChangedSchema.GraphSchemaHash = TEXT("schema_hash_2");
	TestFalse(TEXT("schema mismatch misses"), Cache.TryGet(ChangedSchema, Now, Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePlanCache_PrunesTtlAndCapacity,
	"BlueprintHelper.TaskRuntime.GraphWritePlanCache.PrunesTtlAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWritePlanCache_PrunesTtlAndCapacity::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeCacheConfig Config = FBlueprintHelperTaskRuntimeCacheConfig::Default();
	Config.GraphWritePlanMaxEntries = 1;
	FBlueprintHelperGraphWritePlanCache Cache(Config);

	const FDateTime Now = FDateTime::UtcNow();
	FBlueprintHelperGraphWritePlanCacheKey FirstKey =
		FBlueprintHelperGraphWritePlanCacheTestsLocalUtils::MakeKey();
	FBlueprintHelperGraphWritePlanCacheKey SecondKey = FirstKey;
	SecondKey.PayloadHash = TEXT("payload_hash_2");

	Cache.Store(FirstKey, FBlueprintHelperGraphWritePlanCacheTestsLocalUtils::MakeEntry(), Now);
	Cache.Store(SecondKey, FBlueprintHelperGraphWritePlanCacheTestsLocalUtils::MakeEntry(), Now);

	FBlueprintHelperGraphWritePlanCacheEntry Found;
	TestFalse(TEXT("oldest entry pruned by capacity"), Cache.TryGet(FirstKey, Now, Found));
	TestTrue(TEXT("newest entry remains"), Cache.TryGet(SecondKey, Now, Found));

	const FDateTime Expired = Now + FTimespan::FromSeconds(91.0);
	TestFalse(TEXT("entry expires after ttl"), Cache.TryGet(SecondKey, Expired, Found));
	return true;
}

#endif
