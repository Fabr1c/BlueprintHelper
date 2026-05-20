#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCacheConfig_DefaultsAreCentralized,
	"BlueprintHelper.TaskRuntime.CacheConfig.DefaultsAreCentralized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCacheConfig_DefaultsAreCentralized::RunTest(const FString& Parameters)
{
	const FBlueprintHelperTaskRuntimeCacheConfig Config =
		FBlueprintHelperTaskRuntimeCacheConfig::Default();

	TestEqual(TEXT("partial preview ttl seconds"), Config.PartialPreviewTtl.GetTotalSeconds(), 40.0);
	TestEqual(TEXT("partial preview groups"), Config.PartialPreviewMaxGroups, 64);
	TestEqual(TEXT("partial preview step entries"), Config.PartialPreviewMaxStepEntries, 512);
	TestEqual(TEXT("partial preview max bytes"), Config.PartialPreviewMaxBytes, int64(8) * 1024 * 1024);
	TestEqual(TEXT("call function ttl seconds"), Config.CallFunctionFactTtl.GetTotalSeconds(), 180.0);
	TestEqual(TEXT("call function max entries"), Config.CallFunctionFactMaxEntries, 2048);
	TestEqual(TEXT("graph write plan ttl seconds"), Config.GraphWritePlanTtl.GetTotalSeconds(), 90.0);
	TestEqual(TEXT("graph write plan max bytes"), Config.GraphWritePlanMaxBytes, int64(16) * 1024 * 1024);
	return true;
}

#endif
