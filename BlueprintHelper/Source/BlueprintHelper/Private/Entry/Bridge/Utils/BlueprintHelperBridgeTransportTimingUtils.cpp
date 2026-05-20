// BlueprintHelper Bridge transport timing helpers.

#include "Entry/Bridge/Utils/BlueprintHelperBridgeTransportTimingUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

bool FBlueprintHelperBridgeTransportTimingUtils::ShouldIncludeTiming(
	const TOptional<FBlueprintHelperBridgeRequest>& Request)
{
	if (!Request.IsSet() || !Request.GetValue().Payload.IsValid())
	{
		return false;
	}

	bool bIncludeTiming = false;
	return Request.GetValue().Payload->TryGetBoolField(TEXT("include_timing"), bIncludeTiming) && bIncludeTiming;
}

FBlueprintHelperBridgeTransportTimingUtils::FTimingTrace FBlueprintHelperBridgeTransportTimingUtils::StartTrace(
	const TOptional<FBlueprintHelperBridgeRequest>& Request,
	double StartedAtSeconds)
{
	FTimingTrace Trace;
	Trace.Source = TEXT("ue_bridge_transport");
	Trace.StartedAtSeconds = StartedAtSeconds;

	if (!ShouldIncludeTiming(Request))
	{
		return Trace;
	}

	Trace.bEnabled = true;
	Trace.Operation = Request.GetValue().Command;
	Trace.TimingId = FString::Printf(
		TEXT("timing_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	return Trace;
}

double FBlueprintHelperBridgeTransportTimingUtils::StartStage(const FTimingTrace& Trace)
{
	return Trace.bEnabled ? FPlatformTime::Seconds() : 0.0;
}

void FBlueprintHelperBridgeTransportTimingUtils::AddStage(
	FTimingTrace& Trace,
	const FString& Name,
	double StageStartedAtSeconds,
	double StageFinishedAtSeconds)
{
	if (!Trace.bEnabled)
	{
		return;
	}

	FTimingStage Stage;
	Stage.Name = Name;
	Stage.StartedAtMs = RoundMilliseconds(ToMilliseconds(StageStartedAtSeconds - Trace.StartedAtSeconds));
	Stage.DurationMs = RoundMilliseconds(ToMilliseconds(StageFinishedAtSeconds - StageStartedAtSeconds));
	Trace.Stages.Add(MoveTemp(Stage));
}

TSharedRef<FJsonObject> FBlueprintHelperBridgeTransportTimingUtils::ToJson(const FTimingTrace& Trace)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TimingTrace.v1"));
	Json->SetStringField(TEXT("source"), Trace.Source);
	Json->SetStringField(TEXT("operation"), Trace.Operation);
	Json->SetStringField(TEXT("timing_id"), Trace.TimingId);
	Json->SetNumberField(
		TEXT("total_ms"),
		RoundMilliseconds(ToMilliseconds(FPlatformTime::Seconds() - Trace.StartedAtSeconds)));

	TArray<TSharedPtr<FJsonValue>> StageValues;
	for (const FTimingStage& Stage : Trace.Stages)
	{
		TSharedRef<FJsonObject> StageJson = MakeShared<FJsonObject>();
		StageJson->SetStringField(TEXT("name"), Stage.Name);
		StageJson->SetNumberField(TEXT("started_at_ms"), Stage.StartedAtMs);
		StageJson->SetNumberField(TEXT("duration_ms"), Stage.DurationMs);
		StageValues.Add(MakeShared<FJsonValueObject>(StageJson));
	}
	Json->SetArrayField(TEXT("stages"), StageValues);
	return Json;
}

FString FBlueprintHelperBridgeTransportTimingUtils::SerializeResponseWithTiming(
	FBlueprintHelperBridgeResponse Response,
	FTimingTrace& Trace)
{
	if (!Trace.bEnabled)
	{
		return FBlueprintHelperBridgeProtocol::SerializeResponse(Response);
	}

	const double SerializeStageStart = FPlatformTime::Seconds();
	const FString SerializedWithoutTransportTiming = FBlueprintHelperBridgeProtocol::SerializeResponse(Response);
	const double SerializeStageEnd = FPlatformTime::Seconds();
	(void)SerializedWithoutTransportTiming;

	AddStage(Trace, TEXT("bridge.response_serialize"), SerializeStageStart, SerializeStageEnd);
	Response.TransportTiming = ToJson(Trace);
	return FBlueprintHelperBridgeProtocol::SerializeResponse(Response);
}

double FBlueprintHelperBridgeTransportTimingUtils::RoundMilliseconds(double Value)
{
	return FMath::RoundToDouble(Value * 1000.0) / 1000.0;
}

double FBlueprintHelperBridgeTransportTimingUtils::ToMilliseconds(double Seconds)
{
	return Seconds * 1000.0;
}
