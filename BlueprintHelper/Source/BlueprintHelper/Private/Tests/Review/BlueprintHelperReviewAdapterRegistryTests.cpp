#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/Review/BlueprintHelperReviewAdapterRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAdapterRegistryMissingRestoreReturnsUnavailableTest,
	"BlueprintHelper.Review.AdapterRegistry.MissingRestoreReturnsUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAdapterRegistryMissingRestoreReturnsUnavailableTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FBlueprintHelperReviewAdapterRegistry> Registry =
		FBlueprintHelperReviewAdapterRegistry::CreateDefault();
	const FBlueprintHelperReviewRestoreAdapterLookup Lookup =
		Registry->FindRestoreAdapter(TEXT("unknown_test_target"));

	TestFalse(TEXT("unknown target kind has no restore adapter"), Lookup.bAvailable);
	TestTrue(TEXT("unknown target kind returns diagnostics"), Lookup.Diagnostics.Num() > 0);
	if (Lookup.Diagnostics.Num() > 0)
	{
		TestEqual(
			TEXT("diagnostic code identifies missing restore adapter"),
			Lookup.Diagnostics[0].Code,
			FString(TEXT("restore_adapter_unavailable")));
	}
	return true;
}

#endif
