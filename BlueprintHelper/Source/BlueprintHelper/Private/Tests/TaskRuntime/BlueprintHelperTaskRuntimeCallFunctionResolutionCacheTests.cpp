#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_TracksHitsAndMisses,
	"BlueprintHelper.TaskRuntime.CallFunctionResolutionCache.TracksHitsAndMisses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_TracksHitsAndMisses::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCache Cache;
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Value;
	TestFalse(TEXT("empty cache misses"), Cache.TryGet(TEXT("key"), Value));

	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Stored;
	Stored.bResolved = true;
	Stored.StableId = TEXT("KismetSystemLibrary.PrintString");
	Stored.NativeName = TEXT("PrintString");
	Stored.DisplayName = TEXT("Print String");
	Stored.OwnerClassPath = TEXT("/Script/Engine.KismetSystemLibrary");
	Cache.Store(TEXT("key"), Stored);

	TestTrue(TEXT("stored key hits"), Cache.TryGet(TEXT("key"), Value));
	TestTrue(TEXT("cached value is resolved"), Value.bResolved);
	TestEqual(TEXT("cached stable id"), Value.StableId, Stored.StableId);

	const FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats Stats = Cache.GetStats();
	TestEqual(TEXT("hit count"), Stats.Hits, 1);
	TestEqual(TEXT("miss count"), Stats.Misses, 1);
	TestEqual(TEXT("entry count"), Stats.Entries, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_KeyIncludesContext,
	"BlueprintHelper.TaskRuntime.CallFunctionResolutionCache.KeyIncludesContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_KeyIncludesContext::RunTest(const FString& Parameters)
{
	FBlueprintHelperCallFunctionResolveRequest First;
	First.Query = TEXT("PrintString");
	First.SearchMode = TEXT("exact");
	First.ArgumentNames = {TEXT("InString")};
	First.ArgumentTypes.Add(TEXT("InString"), TEXT("string"));
	First.TargetObjectType = TEXT("self");

	FBlueprintHelperCallFunctionResolveRequest Second = First;
	Second.ArgumentTypes.Add(TEXT("InString"), TEXT("text"));

	const FString FirstKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(First, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	const FString SecondKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(Second, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	const FString ThirdKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(First, TEXT("/Game/BP_B.BP_B"), TEXT("EventGraph"));

	TestFalse(TEXT("different argument types produce different keys"), FirstKey == SecondKey);
	TestFalse(TEXT("different asset paths produce different keys"), FirstKey == ThirdKey);

	FBlueprintHelperCallFunctionResolveRequest PriorityFirst = First;
	PriorityFirst.CategoryPriority.Add(TEXT("Gameplay"));
	PriorityFirst.CategoryPriority.Add(TEXT("Utilities"));

	FBlueprintHelperCallFunctionResolveRequest PrioritySecond = First;
	PrioritySecond.CategoryPriority.Add(TEXT("Utilities"));
	PrioritySecond.CategoryPriority.Add(TEXT("Gameplay"));

	const FString PriorityFirstKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(PriorityFirst, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	const FString PrioritySecondKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(PrioritySecond, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	TestFalse(TEXT("category priority order produces different keys"), PriorityFirstKey == PrioritySecondKey);

	FBlueprintHelperCallFunctionResolveRequest PinFirst = First;
	PinFirst.ArgumentPinTypes.Add(TEXT("InString"), FBlueprintHelperCallFunctionPinType{ TEXT("string") });

	FBlueprintHelperCallFunctionResolveRequest PinSecond = First;
	PinSecond.ArgumentPinTypes.Add(TEXT("InString"), FBlueprintHelperCallFunctionPinType{ TEXT("text") });

	const FString PinFirstKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(PinFirst, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	const FString PinSecondKey =
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(PinSecond, TEXT("/Game/BP_A.BP_A"), TEXT("EventGraph"));
	TestFalse(TEXT("argument pin type produces different keys"), PinFirstKey == PinSecondKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_TtlAndAssetState,
	"BlueprintHelper.TaskRuntime.CallFunctionResolutionCache.TtlAndAssetState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache_TtlAndAssetState::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCache Cache(FBlueprintHelperTaskRuntimeCacheConfig::Default());
	const FString Key = TEXT("call_key");
	const FDateTime Now = FDateTime::UtcNow();

	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Stored;
	Stored.bResolved = true;
	Stored.StableId = TEXT("/Script/Engine.KismetSystemLibrary:PrintString");
	Stored.NativeName = TEXT("PrintString");
	Stored.DisplayName = TEXT("Print String");
	Stored.OwnerClassPath = TEXT("/Script/Engine.KismetSystemLibrary");
	Stored.AssetStateHash = TEXT("asset_v1");
	Cache.Store(Key, Stored, Now);

	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Found;
	TestTrue(TEXT("hit before ttl"), Cache.TryGet(Key, TEXT("asset_v1"), Now, Found));
	TestFalse(TEXT("asset hash mismatch misses"), Cache.TryGet(Key, TEXT("asset_v2"), Now, Found));
	TestFalse(
		TEXT("expired entry misses"),
		Cache.TryGet(Key, TEXT("asset_v1"), Now + FTimespan::FromSeconds(181.0), Found));
	return true;
}

#endif
