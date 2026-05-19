// BlueprintHelper TaskRuntime timing trace helpers.

#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeTimingUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace FBlueprintHelperTaskRuntimeTimingUtils::StartTrace(
	const FString& Operation,
	bool bEnabled)
{
	FTimingTrace Trace;
	Trace.bEnabled = bEnabled;
	Trace.Source = TEXT("ue_task_runtime");
	Trace.Operation = Operation;
	if (!bEnabled)
	{
		return Trace;
	}

	Trace.TimingId = FString::Printf(
		TEXT("timing_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	Trace.StartedAtSeconds = FPlatformTime::Seconds();
	return Trace;
}

double FBlueprintHelperTaskRuntimeTimingUtils::StartStage(const FTimingTrace& Trace)
{
	return Trace.bEnabled ? FPlatformTime::Seconds() : 0.0;
}

void FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
	FTimingTrace& Trace,
	const FString& Name,
	double StartedAtSeconds)
{
	if (!Trace.bEnabled)
	{
		return;
	}

	const double FinishedAtSeconds = FPlatformTime::Seconds();
	FTimingStage Stage;
	Stage.Name = Name;
	Stage.StartedAtMs = RoundMilliseconds(ToMilliseconds(StartedAtSeconds - Trace.StartedAtSeconds));
	Stage.DurationMs = RoundMilliseconds(ToMilliseconds(FinishedAtSeconds - StartedAtSeconds));
	Trace.Stages.Add(MoveTemp(Stage));
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeTimingUtils::ToJson(
	const FTimingTrace& Trace)
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

void FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(
	const TSharedPtr<FJsonObject>& Data,
	const FTimingTrace& Trace)
{
	if (!Trace.bEnabled || !Data.IsValid())
	{
		return;
	}

	Data->SetObjectField(TEXT("timing"), ToJson(Trace));
}

double FBlueprintHelperTaskRuntimeTimingUtils::RoundMilliseconds(double Value)
{
	return FMath::RoundToDouble(Value * 1000.0) / 1000.0;
}

double FBlueprintHelperTaskRuntimeTimingUtils::ToMilliseconds(double Seconds)
{
	return Seconds * 1000.0;
}
