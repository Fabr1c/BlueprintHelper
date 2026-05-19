// BlueprintHelper TaskRuntime timing trace helpers.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FBlueprintHelperTaskRuntimeTimingUtils
{
public:
	struct FTimingStage
	{
		FString Name;
		double StartedAtMs = 0.0;
		double DurationMs = 0.0;
	};

	struct FTimingTrace
	{
		FString Source;
		FString Operation;
		FString TimingId;
		bool bEnabled = false;
		double StartedAtSeconds = 0.0;
		TArray<FTimingStage> Stages;
	};

	static FTimingTrace StartTrace(const FString& Operation, bool bEnabled);
	static double StartStage(const FTimingTrace& Trace);
	static void FinishStage(FTimingTrace& Trace, const FString& Name, double StartedAtSeconds);
	static TSharedRef<FJsonObject> ToJson(const FTimingTrace& Trace);
	static void AttachTiming(const TSharedPtr<FJsonObject>& Data, const FTimingTrace& Trace);

private:
	static double RoundMilliseconds(double Value);
	static double ToMilliseconds(double Seconds);
};
