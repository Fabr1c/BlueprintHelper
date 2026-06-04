// BlueprintHelper Metrics store reader implementation.

#include "Systems/Metrics/BlueprintHelperMetricsStoreReader.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static FString BlueprintHelperMetricsReadOptionalString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
	}
	return Value;
}

static bool BlueprintHelperMetricsReadOptionalNonNegativeInt64(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	int64& OutValue)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	const TSharedPtr<FJsonValue> Field = Object->TryGetField(FieldName);
	double NumberValue = 0.0;
	if (!Field.IsValid() || Field->Type != EJson::Number || !Field->TryGetNumber(NumberValue))
	{
		return false;
	}
	if (!FMath::IsFinite(NumberValue) ||
		NumberValue < 0.0 ||
		NumberValue >= 9223372036854775808.0 ||
		NumberValue != FMath::FloorToDouble(NumberValue))
	{
		return false;
	}

	OutValue = static_cast<int64>(NumberValue);
	return true;
}

static void BlueprintHelperMetricsParseTaskKey(
	const TSharedPtr<FJsonObject>& TaskKeyObject,
	FBlueprintHelperMetricsEvent& OutEvent)
{
	if (!TaskKeyObject.IsValid())
	{
		return;
	}

	OutEvent.TaskKey.TaskType =
		BlueprintHelperMetricsReadOptionalString(TaskKeyObject, TEXT("task_type"));
	OutEvent.TaskKey.FeatureName =
		BlueprintHelperMetricsReadOptionalString(TaskKeyObject, TEXT("feature_name"));
	OutEvent.TaskKey.TargetType =
		BlueprintHelperMetricsReadOptionalString(TaskKeyObject, TEXT("target_type"));
	OutEvent.TaskKey.TargetRefHash =
		BlueprintHelperMetricsReadOptionalString(TaskKeyObject, TEXT("target_ref_hash"));
	OutEvent.TaskKey.TargetRefLabel =
		BlueprintHelperMetricsReadOptionalString(TaskKeyObject, TEXT("target_ref_label"));
	OutEvent.bHasTaskKey = true;
}

static void BlueprintHelperMetricsParseIssue(
	const TSharedPtr<FJsonObject>& IssueObject,
	FBlueprintHelperMetricsEvent& OutEvent)
{
	if (!IssueObject.IsValid())
	{
		return;
	}

	OutEvent.Issue.Code =
		BlueprintHelperMetricsReadOptionalString(IssueObject, TEXT("code"));
	OutEvent.Issue.Path =
		BlueprintHelperMetricsReadOptionalString(IssueObject, TEXT("path"));
	OutEvent.Issue.MessageDigest =
		BlueprintHelperMetricsReadOptionalString(IssueObject, TEXT("message_digest"));
	OutEvent.bHasIssue = true;
}

static bool BlueprintHelperMetricsParseIo(
	const TSharedPtr<FJsonObject>& IoObject,
	FBlueprintHelperMetricsEvent& OutEvent)
{
	if (!IoObject.IsValid())
	{
		return true;
	}

	OutEvent.Io.InputSource =
		BlueprintHelperMetricsReadOptionalString(IoObject, TEXT("input_source"));
	if (!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("input_chars"),
			OutEvent.Io.InputChars) ||
		!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("input_utf8_bytes"),
			OutEvent.Io.InputUtf8Bytes) ||
		!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("output_chars"),
			OutEvent.Io.OutputChars) ||
		!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("output_utf8_bytes"),
			OutEvent.Io.OutputUtf8Bytes) ||
		!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("estimated_input_tokens"),
			OutEvent.Io.EstimatedInputTokens) ||
		!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			IoObject,
			TEXT("estimated_output_tokens"),
			OutEvent.Io.EstimatedOutputTokens))
	{
		return false;
	}

	OutEvent.bHasIo = true;
	return true;
}

static bool BlueprintHelperMetricsParseEventLine(
	const FString& Line,
	FBlueprintHelperMetricsEvent& OutEvent)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return false;
	}

	OutEvent.Timestamp = BlueprintHelperMetricsReadOptionalString(Object, TEXT("timestamp"));
	OutEvent.EventType = BlueprintHelperMetricsReadOptionalString(Object, TEXT("event_type"));
	OutEvent.ToolName = BlueprintHelperMetricsReadOptionalString(Object, TEXT("tool_name"));
	OutEvent.Status = BlueprintHelperMetricsReadOptionalString(Object, TEXT("status"));
	OutEvent.ErrorCategory =
		BlueprintHelperMetricsReadOptionalString(Object, TEXT("error_category"));
	OutEvent.ErrorCode = BlueprintHelperMetricsReadOptionalString(Object, TEXT("error_code"));
	OutEvent.Capability = BlueprintHelperMetricsReadOptionalString(Object, TEXT("capability"));
	OutEvent.SemanticOperation =
		BlueprintHelperMetricsReadOptionalString(Object, TEXT("semantic_operation"));
	if (!BlueprintHelperMetricsReadOptionalNonNegativeInt64(
			Object,
			TEXT("duration_ms"),
			OutEvent.DurationMs))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* TaskKeyObject = nullptr;
	if (Object->TryGetObjectField(TEXT("task_key"), TaskKeyObject) && TaskKeyObject != nullptr)
	{
		BlueprintHelperMetricsParseTaskKey(*TaskKeyObject, OutEvent);
	}

	const TSharedPtr<FJsonObject>* IssueObject = nullptr;
	if (Object->TryGetObjectField(TEXT("issue"), IssueObject) && IssueObject != nullptr)
	{
		BlueprintHelperMetricsParseIssue(*IssueObject, OutEvent);
	}

	const TSharedPtr<FJsonObject>* IoObject = nullptr;
	if (Object->TryGetObjectField(TEXT("io"), IoObject) && IoObject != nullptr)
	{
		if (!BlueprintHelperMetricsParseIo(*IoObject, OutEvent))
		{
			return false;
		}
	}

	return true;
}

FString FBlueprintHelperMetricsStoreReader::ResolveDefaultMetricsRoot()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Metrics");
}

FString FBlueprintHelperMetricsStoreReader::ResolveMetricsRootFromEnvironment()
{
	const FString OverrideRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("BPH_METRICS_DIR")).TrimStartAndEnd();
	return OverrideRoot.IsEmpty()
		? ResolveDefaultMetricsRoot()
		: FPaths::ConvertRelativePathToFull(OverrideRoot);
}

FBlueprintHelperMetricsLoadResult FBlueprintHelperMetricsStoreReader::LoadDefault()
{
	return LoadFromRoot(ResolveMetricsRootFromEnvironment());
}

FBlueprintHelperMetricsLoadResult FBlueprintHelperMetricsStoreReader::LoadFromRoot(
	const FString& MetricsRoot)
{
	FBlueprintHelperMetricsLoadResult Result;
	Result.MetricsRoot = FPaths::ConvertRelativePathToFull(MetricsRoot);

	const FString EventsDir = Result.MetricsRoot / TEXT("events");
	if (!IFileManager::Get().DirectoryExists(*EventsDir))
	{
		return Result;
	}

	TArray<FString> JsonlFileNames;
	IFileManager::Get().FindFiles(JsonlFileNames, *(EventsDir / TEXT("*.jsonl")), true, false);
	JsonlFileNames.Sort();

	for (const FString& FileName : JsonlFileNames)
	{
		const FString FilePath = EventsDir / FileName;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			Result.bSucceeded = false;
			Result.Error = FString::Printf(TEXT("metrics_file_read_failed:%s"), *FileName);
			return Result;
		}

		++Result.FilesRead;

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);
		Result.LinesRead += Lines.Num();

		for (const FString& Line : Lines)
		{
			FBlueprintHelperMetricsEvent Event;
			if (!BlueprintHelperMetricsParseEventLine(Line, Event))
			{
				++Result.ParseWarnings;
				++Result.LinesSkipped;
				continue;
			}

			Result.Events.Add(MoveTemp(Event));
		}
	}

	return Result;
}
