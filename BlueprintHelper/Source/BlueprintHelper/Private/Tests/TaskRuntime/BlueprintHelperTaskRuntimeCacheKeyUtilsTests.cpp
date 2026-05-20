#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheKeyUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCacheKeyUtils_StableJsonHash,
	"BlueprintHelper.TaskRuntime.CacheKeyUtils.StableJsonHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCacheKeyUtils_StableJsonHash::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> First = MakeShared<FJsonObject>();
	First->SetStringField(TEXT("b"), TEXT("2"));
	First->SetStringField(TEXT("a"), TEXT("1"));

	TSharedRef<FJsonObject> Second = MakeShared<FJsonObject>();
	Second->SetStringField(TEXT("a"), TEXT("1"));
	Second->SetStringField(TEXT("b"), TEXT("2"));

	const FString FirstHash = FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(First);
	const FString SecondHash = FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(Second);
	TestEqual(TEXT("object field order does not change hash"), FirstHash, SecondHash);

	Second->SetStringField(TEXT("b"), TEXT("3"));
	TestFalse(
		TEXT("value change changes hash"),
		FirstHash == FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(Second));
	return true;
}

#endif
