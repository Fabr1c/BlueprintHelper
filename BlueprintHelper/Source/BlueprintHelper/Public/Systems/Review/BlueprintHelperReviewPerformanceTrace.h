// BlueprintHelper Review performance trace helpers.

#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPerformanceScope
{
public:
	explicit FBlueprintHelperReviewPerformanceScope(
		const TCHAR* InName,
		int32 InWarnThresholdMs = 16);
	~FBlueprintHelperReviewPerformanceScope();

	void AddCount(const TCHAR* Key, int64 Value);
	void AddBytes(const TCHAR* Key, int64 Value);
	int32 GetWarnThresholdMs() const;
	double GetElapsedMilliseconds() const;
	FString GetCounterText() const;

private:
	const TCHAR* Name = TEXT("ReviewPerf");
	double StartSeconds = 0.0;
	int32 WarnThresholdMs = 16;
	TArray<FString> Counters;
};
