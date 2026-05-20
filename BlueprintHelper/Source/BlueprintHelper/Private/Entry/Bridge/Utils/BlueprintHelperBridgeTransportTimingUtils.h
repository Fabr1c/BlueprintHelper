// BlueprintHelper Bridge transport timing helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Bridge/BlueprintHelperBridgeTypes.h"

class FJsonObject;

class FBlueprintHelperBridgeTransportTimingUtils
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

	static bool ShouldIncludeTiming(const TOptional<FBlueprintHelperBridgeRequest>& Request);
	static FTimingTrace StartTrace(
		const TOptional<FBlueprintHelperBridgeRequest>& Request,
		double StartedAtSeconds);
	static double StartStage(const FTimingTrace& Trace);
	static void AddStage(
		FTimingTrace& Trace,
		const FString& Name,
		double StageStartedAtSeconds,
		double StageFinishedAtSeconds);
	static TSharedRef<FJsonObject> ToJson(const FTimingTrace& Trace);
	static FString SerializeResponseWithTiming(
		FBlueprintHelperBridgeResponse Response,
		FTimingTrace& Trace);

private:
	static double RoundMilliseconds(double Value);
	static double ToMilliseconds(double Seconds);
};
