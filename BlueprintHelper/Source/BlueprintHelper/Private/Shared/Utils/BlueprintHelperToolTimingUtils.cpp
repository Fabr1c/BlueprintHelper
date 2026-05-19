// BlueprintHelper generic tool timing trace helpers.

#include "Shared/Utils/BlueprintHelperToolTimingUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

static thread_local FBlueprintHelperToolTimingUtils::FTimingTrace* GBlueprintHelperCurrentToolTimingTrace = nullptr;

FBlueprintHelperToolTimingUtils::FTimingTrace FBlueprintHelperToolTimingUtils::StartTrace(
	const FString& Operation,
	bool bEnabled,
	const FString& Source)
{
	FTimingTrace Trace;
	Trace.bEnabled = bEnabled;
	Trace.Source = Source;
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

double FBlueprintHelperToolTimingUtils::StartStage(const FTimingTrace& Trace)
{
	return Trace.bEnabled ? FPlatformTime::Seconds() : 0.0;
}

void FBlueprintHelperToolTimingUtils::FinishStage(
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

void FBlueprintHelperToolTimingUtils::AddCounter(
	FTimingTrace& Trace,
	const FString& Name,
	int64 Value)
{
	if (!Trace.bEnabled)
	{
		return;
	}

	FTimingStage Stage;
	Stage.Name = Name;
	Stage.StartedAtMs = RoundMilliseconds(ToMilliseconds(FPlatformTime::Seconds() - Trace.StartedAtSeconds));
	Stage.DurationMs = 0.0;
	Stage.Value = static_cast<double>(Value);
	Trace.Stages.Add(MoveTemp(Stage));
}

TSharedRef<FJsonObject> FBlueprintHelperToolTimingUtils::ToJson(
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
		if (Stage.Value.IsSet())
		{
			StageJson->SetNumberField(TEXT("value"), Stage.Value.GetValue());
		}
		StageValues.Add(MakeShared<FJsonValueObject>(StageJson));
	}
	Json->SetArrayField(TEXT("stages"), StageValues);
	return Json;
}

void FBlueprintHelperToolTimingUtils::AttachTiming(
	const TSharedPtr<FJsonObject>& Data,
	const FTimingTrace& Trace)
{
	if (!Trace.bEnabled || !Data.IsValid() || Data->HasField(TEXT("timing")))
	{
		return;
	}

	Data->SetObjectField(TEXT("timing"), ToJson(Trace));
}

void FBlueprintHelperToolTimingUtils::AttachTimingToBridgeResult(
	TSharedPtr<FJsonObject>& Result,
	const FTimingTrace& Trace)
{
	if (!Trace.bEnabled || !Result.IsValid())
	{
		return;
	}

	FString Schema;
	if (Result->TryGetStringField(TEXT("schema"), Schema) &&
		Schema == TEXT("BlueprintHelper.ToolResult.v1"))
	{
		TSharedPtr<FJsonObject> Data;
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (!Result->TryGetObjectField(TEXT("data"), DataPtr) || DataPtr == nullptr || !DataPtr->IsValid())
		{
			Data = MakeShared<FJsonObject>();
			Result->SetObjectField(TEXT("data"), Data);
		}
		else
		{
			Data = *DataPtr;
		}
		AttachTiming(Data, Trace);
		return;
	}

	AttachTiming(Result, Trace);
}

FBlueprintHelperToolTimingUtils::FTimingTrace* FBlueprintHelperToolTimingUtils::GetCurrentTrace()
{
	return GBlueprintHelperCurrentToolTimingTrace;
}

void FBlueprintHelperToolTimingUtils::SetCurrentTrace(FTimingTrace* Trace)
{
	GBlueprintHelperCurrentToolTimingTrace = Trace;
}

double FBlueprintHelperToolTimingUtils::RoundMilliseconds(double Value)
{
	return FMath::RoundToDouble(Value * 1000.0) / 1000.0;
}

double FBlueprintHelperToolTimingUtils::ToMilliseconds(double Seconds)
{
	return Seconds * 1000.0;
}
