// BlueprintHelper Review performance trace helpers implementation.

#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"

#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperReviewPerf, Log, All);

FBlueprintHelperReviewPerformanceScope::FBlueprintHelperReviewPerformanceScope(
	const TCHAR* InName,
	int32 InWarnThresholdMs)
	: Name(InName ? InName : TEXT("ReviewPerf"))
	, StartSeconds(FPlatformTime::Seconds())
	, WarnThresholdMs(FMath::Max(0, InWarnThresholdMs))
{
}

FBlueprintHelperReviewPerformanceScope::~FBlueprintHelperReviewPerformanceScope()
{
	const double ElapsedMs = GetElapsedMilliseconds();
	const FString CounterText = GetCounterText();
	if (ElapsedMs >= WarnThresholdMs)
	{
		UE_LOG(LogBlueprintHelperReviewPerf, Warning, TEXT("%s ms=%.2f %s"), Name, ElapsedMs, *CounterText);
		return;
	}

	UE_LOG(LogBlueprintHelperReviewPerf, Verbose, TEXT("%s ms=%.2f %s"), Name, ElapsedMs, *CounterText);
}

void FBlueprintHelperReviewPerformanceScope::AddCount(const TCHAR* Key, int64 Value)
{
	Counters.Add(FString::Printf(TEXT("%s=%lld"), Key ? Key : TEXT("count"), Value));
}

void FBlueprintHelperReviewPerformanceScope::AddBytes(const TCHAR* Key, int64 Value)
{
	Counters.Add(FString::Printf(TEXT("%s_bytes=%lld"), Key ? Key : TEXT("value"), Value));
}

int32 FBlueprintHelperReviewPerformanceScope::GetWarnThresholdMs() const
{
	return WarnThresholdMs;
}

double FBlueprintHelperReviewPerformanceScope::GetElapsedMilliseconds() const
{
	return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
}

FString FBlueprintHelperReviewPerformanceScope::GetCounterText() const
{
	return FString::Join(Counters, TEXT(" "));
}
