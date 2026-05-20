#include "Runtime/TaskRuntime/BlueprintHelperTaskPartialPreviewCache.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils
{
public:
	static FBlueprintHelperPartialPreviewCacheKey MakeKey(const FString& StepId = TEXT("step_001"))
	{
		FBlueprintHelperPartialPreviewCacheKey Key;
		Key.TaskSpecGroupHash = TEXT("group_a");
		Key.StepId = StepId;
		Key.StepPayloadHash = TEXT("payload_hash");
		Key.DependencyClosureHash = TEXT("deps_hash");
		Key.ExecutionPolicyHash = TEXT("policy_hash");
		Key.AssetStateHash = TEXT("asset_state_hash");
		return Key;
	}

	static FBlueprintHelperPartialPreviewCacheEntry MakeEntry(const FString& StepId = TEXT("step_001"))
	{
		FBlueprintHelperPartialPreviewCacheEntry Entry;
		Entry.StepId = StepId;
		Entry.bPassed = true;
		Entry.Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("graph_write"),
			TEXT("trace_cache"));
		return Entry;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPartialPreviewCache_HitsOnlyForMatchingKey,
	"BlueprintHelper.TaskRuntime.PartialPreviewCache.HitsOnlyForMatchingKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskPartialPreviewCache_HitsOnlyForMatchingKey::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPartialPreviewCache Cache(FBlueprintHelperTaskRuntimeCacheConfig::Default());
	const FDateTime Now = FDateTime::UtcNow();
	const FBlueprintHelperPartialPreviewCacheKey Key =
		FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeKey();
	Cache.Store(Key, FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeEntry(), Now);

	FBlueprintHelperPartialPreviewCacheEntry Found;
	TestTrue(TEXT("matching key hits"), Cache.TryGet(Key, Now, Found));

	FBlueprintHelperPartialPreviewCacheKey PayloadChanged = Key;
	PayloadChanged.StepPayloadHash = TEXT("payload_changed");
	TestFalse(TEXT("changed payload misses"), Cache.TryGet(PayloadChanged, Now, Found));

	FBlueprintHelperPartialPreviewCacheKey DependencyChanged = Key;
	DependencyChanged.DependencyClosureHash = TEXT("deps_changed");
	TestFalse(TEXT("changed dependency closure misses"), Cache.TryGet(DependencyChanged, Now, Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPartialPreviewCache_PrunesTtlAndCapacity,
	"BlueprintHelper.TaskRuntime.PartialPreviewCache.PrunesTtlAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskPartialPreviewCache_PrunesTtlAndCapacity::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeCacheConfig Config = FBlueprintHelperTaskRuntimeCacheConfig::Default();
	Config.PartialPreviewMaxStepEntries = 1;
	FBlueprintHelperTaskPartialPreviewCache Cache(Config);

	const FDateTime Now = FDateTime::UtcNow();
	FBlueprintHelperPartialPreviewCacheKey FirstKey =
		FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeKey(TEXT("step_001"));
	FBlueprintHelperPartialPreviewCacheKey SecondKey =
		FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeKey(TEXT("step_002"));
	SecondKey.StepPayloadHash = TEXT("payload_hash_2");

	Cache.Store(FirstKey, FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeEntry(TEXT("step_001")), Now);
	Cache.Store(SecondKey, FBlueprintHelperTaskPartialPreviewCacheTestsLocalUtils::MakeEntry(TEXT("step_002")), Now);

	FBlueprintHelperPartialPreviewCacheEntry Found;
	TestFalse(TEXT("oldest entry pruned by capacity"), Cache.TryGet(FirstKey, Now, Found));
	TestTrue(TEXT("newest entry remains"), Cache.TryGet(SecondKey, Now, Found));

	const FDateTime Expired = Now + FTimespan::FromSeconds(41.0);
	TestFalse(TEXT("entry expires after ttl"), Cache.TryGet(SecondKey, Expired, Found));
	return true;
}

#endif
