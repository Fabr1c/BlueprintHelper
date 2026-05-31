#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPerformanceTraceCountersTest,
	"BlueprintHelper.Review.PerformanceTrace.Counters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPerformanceTraceCountersTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPerformanceScope Scope(TEXT("ReviewPerf.Test"), 100000);
	Scope.AddCount(TEXT("records"), 3);
	Scope.AddBytes(TEXT("payload"), 4096);

	TestEqual(TEXT("Warn threshold is readable"), Scope.GetWarnThresholdMs(), 100000);
	TestTrue(TEXT("Count counter is readable"), Scope.GetCounterText().Contains(TEXT("records=3")));
	TestTrue(TEXT("Byte counter is readable"), Scope.GetCounterText().Contains(TEXT("payload_bytes=4096")));
	TestTrue(TEXT("Elapsed time is non-negative"), Scope.GetElapsedMilliseconds() >= 0.0);

	return true;
}

#endif
