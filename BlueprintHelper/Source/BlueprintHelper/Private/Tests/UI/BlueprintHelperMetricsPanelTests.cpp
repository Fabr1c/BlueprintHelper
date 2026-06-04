#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Metrics/BlueprintHelperMetricsStoreReader.h"
#include "Systems/Metrics/BlueprintHelperMetricsTimeSeriesService.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelData.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"
#include "UI/Metrics/SBlueprintHelperMetricsPanel.h"
#include "UI/Metrics/SBlueprintHelperMetricsDetailChart.h"
#include "UI/Metrics/SBlueprintHelperMetricsMetricSelector.h"
#include "UI/Metrics/SBlueprintHelperMetricsOverviewChart.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"

#include <atomic>

class FBlueprintHelperMetricsPanelTestFiles
{
public:
	explicit FBlueprintHelperMetricsPanelTestFiles(const FString& InName)
		: Root(FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("MetricsPanelTests") / InName)
		, EventsDir(Root / TEXT("events"))
	{
		IFileManager::Get().DeleteDirectory(*Root, false, true);
		IFileManager::Get().MakeDirectory(*EventsDir, true);
	}

	~FBlueprintHelperMetricsPanelTestFiles()
	{
		IFileManager::Get().DeleteDirectory(*Root, false, true);
	}

	bool WriteEventFile(FAutomationTestBase& Test, const FString& FileName, const FString& Text) const
	{
		const FString FilePath = EventsDir / FileName;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		const bool bSaved = FFileHelper::SaveStringToFile(
			Text,
			*FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test.TestTrue(*FString::Printf(TEXT("writes %s"), *FileName), bSaved);
		return bSaved;
	}

	FString Root;
	FString EventsDir;
};

class FBlueprintHelperMetricsScopedEnvVar
{
public:
	FBlueprintHelperMetricsScopedEnvVar(const TCHAR* InName, const FString& InValue)
		: Name(InName)
		, PreviousValue(FPlatformMisc::GetEnvironmentVariable(InName))
	{
		FPlatformMisc::SetEnvironmentVar(*Name, *InValue);
	}

	~FBlueprintHelperMetricsScopedEnvVar()
	{
		if (PreviousValue.IsEmpty())
		{
			FPlatformMisc::SetEnvironmentVar(*Name, nullptr);
			return;
		}
		FPlatformMisc::SetEnvironmentVar(*Name, *PreviousValue);
	}

private:
	FString Name;
	FString PreviousValue;
};

static FDateTime BlueprintHelperMetricsMakeUtcDateTime(
	int32 Year,
	int32 Month,
	int32 Day,
	int32 Hour = 0,
	int32 Minute = 0,
	int32 Second = 0)
{
	return FDateTime(Year, Month, Day, Hour, Minute, Second);
}

static FBlueprintHelperMetricsEvent BlueprintHelperMetricsMakeEvent(
	const TCHAR* Timestamp,
	const TCHAR* EventType,
	const TCHAR* ToolName,
	const TCHAR* Status)
{
	FBlueprintHelperMetricsEvent Event;
	Event.Timestamp = Timestamp;
	Event.EventType = EventType;
	Event.ToolName = ToolName;
	Event.Status = Status;
	return Event;
}

static void BlueprintHelperMetricsSetTaskKey(
	FBlueprintHelperMetricsEvent& Event,
	const TCHAR* TaskType,
	const TCHAR* FeatureName,
	const TCHAR* TargetType)
{
	Event.bHasTaskKey = true;
	Event.TaskKey.TaskType = TaskType;
	Event.TaskKey.FeatureName = FeatureName;
	Event.TaskKey.TargetType = TargetType;
	Event.TaskKey.TargetRefHash = TEXT("sha256:test");
	Event.TaskKey.TargetRefLabel = TEXT("/Game/Test/BP_Metrics");
}

static void BlueprintHelperMetricsSetFailure(
	FBlueprintHelperMetricsEvent& Event,
	const TCHAR* ErrorCategory,
	const TCHAR* ErrorCode,
	const TCHAR* IssueCode)
{
	Event.ErrorCategory = ErrorCategory;
	Event.ErrorCode = ErrorCode;
	Event.bHasIssue = true;
	Event.Issue.Code = IssueCode;
}

static void BlueprintHelperMetricsSetIo(
	FBlueprintHelperMetricsEvent& Event,
	int64 InputChars,
	int64 InputUtf8Bytes,
	int64 OutputChars,
	int64 OutputUtf8Bytes,
	int64 EstimatedInputTokens,
	int64 EstimatedOutputTokens)
{
	Event.bHasIo = true;
	Event.Io.InputSource = TEXT("task_file");
	Event.Io.InputChars = InputChars;
	Event.Io.InputUtf8Bytes = InputUtf8Bytes;
	Event.Io.OutputChars = OutputChars;
	Event.Io.OutputUtf8Bytes = OutputUtf8Bytes;
	Event.Io.EstimatedInputTokens = EstimatedInputTokens;
	Event.Io.EstimatedOutputTokens = EstimatedOutputTokens;
}

static void BlueprintHelperMetricsSetOperation(
	FBlueprintHelperMetricsEvent& Event,
	const TCHAR* Capability,
	const TCHAR* SemanticOperation)
{
	Event.Capability = Capability;
	Event.SemanticOperation = SemanticOperation;
}

static FBlueprintHelperMetricsPanelSnapshot BlueprintHelperMetricsMakePresenterSnapshot(
	EBlueprintHelperMetricsTimelineMode TimelineMode,
	EBlueprintHelperMetricsLoadState LoadState,
	int32 TotalEvents,
	const TCHAR* StatusText)
{
	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	Snapshot.TimelineMode = TimelineMode;
	Snapshot.Selection.TimelineMode = TimelineMode;
	Snapshot.LoadState = LoadState;
	Snapshot.Summary.TotalEvents = TotalEvents;
	Snapshot.StatusText = StatusText;
	return Snapshot;
}

static FBlueprintHelperMetricsPanelSnapshot BlueprintHelperMetricsMakeLoadedPresenterSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection,
	int32 TotalEvents,
	const TCHAR* StatusText)
{
	FBlueprintHelperMetricsPanelSnapshot Snapshot =
		BlueprintHelperMetricsMakePresenterSnapshot(
			Selection.TimelineMode,
			EBlueprintHelperMetricsLoadState::Loaded,
			TotalEvents,
			StatusText);
	Snapshot.Selection = Selection;
	Snapshot.SelectedMetricTitle =
		Selection.MetricKind == EBlueprintHelperMetricsMetricKind::OperationUsage
			? TEXT("Operations")
			: TEXT("Tool Calls");

	FBlueprintHelperMetricsMetricOptionView ToolOption;
	ToolOption.Kind = EBlueprintHelperMetricsMetricKind::ToolUsage;
	ToolOption.Label = TEXT("Tool Calls");
	ToolOption.UnitLabel = TEXT("events");
	ToolOption.Total = TotalEvents;
	ToolOption.bIsSelected =
		Selection.MetricKind == EBlueprintHelperMetricsMetricKind::ToolUsage;
	Snapshot.MetricOptions.Add(ToolOption);

	FBlueprintHelperMetricsMetricOptionView OperationOption;
	OperationOption.Kind = EBlueprintHelperMetricsMetricKind::OperationUsage;
	OperationOption.Label = TEXT("Operations");
	OperationOption.UnitLabel = TEXT("events");
	OperationOption.Total = TotalEvents / 2;
	OperationOption.bIsSelected =
		Selection.MetricKind == EBlueprintHelperMetricsMetricKind::OperationUsage;
	Snapshot.MetricOptions.Add(OperationOption);

	const FString SelectedBucketId = Selection.SelectedBucketId.IsEmpty()
		? FString(TEXT("bucket-a"))
		: Selection.SelectedBucketId;
	Snapshot.Selection.SelectedBucketId = SelectedBucketId;
	Snapshot.SelectedBucketLabel = SelectedBucketId;
	Snapshot.SelectedBucketTotal =
		SelectedBucketId == TEXT("bucket-b") ? TotalEvents / 2 : TotalEvents;

	FBlueprintHelperMetricsOverviewBarView FirstBar;
	FirstBar.BucketId = TEXT("bucket-a");
	FirstBar.Label = TEXT("2026-06-03");
	FirstBar.Value = TotalEvents;
	FirstBar.Fraction = 1.0f;
	FirstBar.bIsSelected = SelectedBucketId == FirstBar.BucketId;
	Snapshot.OverviewBars.Add(FirstBar);

	FBlueprintHelperMetricsOverviewBarView SecondBar;
	SecondBar.BucketId = TEXT("bucket-b");
	SecondBar.Label = TEXT("2026-06-04");
	SecondBar.Value = TotalEvents / 2;
	SecondBar.Fraction = 0.5f;
	SecondBar.bIsSelected = SelectedBucketId == SecondBar.BucketId;
	Snapshot.OverviewBars.Add(SecondBar);

	FBlueprintHelperMetricsDetailBarView Detail;
	Detail.Label = SelectedBucketId == TEXT("bucket-b")
		? TEXT("graph_write / append_after")
		: TEXT("blueprinthelper_read_context");
	Detail.Value = Snapshot.SelectedBucketTotal;
	Detail.UnitLabel = TEXT("events");
	Detail.SubText = TEXT("success=1 failed=0");
	Detail.Fraction = 1.0f;
	Snapshot.DetailBars.Add(Detail);

	return Snapshot;
}

static bool BlueprintHelperMetricsPumpAsyncUntil(
	TFunctionRef<bool()> Predicate,
	double TimeoutSeconds = 5.0)
{
	const double DeadlineSeconds = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < DeadlineSeconds)
	{
		FBlueprintHelperMetricsPanelAsyncUtils::FlushTasks();
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(
			ENamedThreads::GameThread);
		if (Predicate())
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}

	FBlueprintHelperMetricsPanelAsyncUtils::FlushTasks();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	return Predicate();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelDataDefaultsTest,
	"BlueprintHelper.UI.MetricsPanel.DataDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelDataDefaultsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	TestEqual(TEXT("default timeline"), Snapshot.TimelineMode, EBlueprintHelperMetricsTimelineMode::Daily);
	TestEqual(TEXT("default load state"), Snapshot.LoadState, EBlueprintHelperMetricsLoadState::Empty);
	TestEqual(TEXT("default bucket count"), Snapshot.Buckets.Num(), 0);
	TestEqual(TEXT("default total events"), Snapshot.Summary.TotalEvents, 0);

	FBlueprintHelperMetricsQuery Query;
	TestEqual(TEXT("default query timeline"), Query.TimelineMode, EBlueprintHelperMetricsTimelineMode::Daily);
	TestEqual(TEXT("default query now"), Query.NowUtc, FDateTime());
	TestEqual(TEXT("default daily buckets"), Query.DailyBucketCount, 14);
	TestEqual(TEXT("default weekly buckets"), Query.WeeklyBucketCount, 8);
	TestEqual(TEXT("default top row limit"), Query.TopRowLimit, 10);

	FBlueprintHelperMetricsLoadResult LoadResult;
	TestTrue(TEXT("default load result succeeds"), LoadResult.bSucceeded);
	TestEqual(TEXT("default load result events"), LoadResult.Events.Num(), 0);

	const FBlueprintHelperMetricsPanelVisualEvent RefreshEvent =
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked();
	TestEqual(TEXT("refresh event type"), RefreshEvent.Type, EBlueprintHelperMetricsVisualEventType::RefreshClicked);

	const FBlueprintHelperMetricsPanelVisualEvent WeeklyEvent =
		FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
			EBlueprintHelperMetricsTimelineMode::Weekly);
	TestEqual(TEXT("weekly event type"), WeeklyEvent.Type, EBlueprintHelperMetricsVisualEventType::TimelineModeChanged);
	TestEqual(TEXT("weekly event mode"), WeeklyEvent.TimelineMode, EBlueprintHelperMetricsTimelineMode::Weekly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelSelectionDefaultsTest,
	"BlueprintHelper.UI.MetricsPanel.Data.SelectionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelSelectionDefaultsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsQuery Query;
	TestEqual(TEXT("default metric kind"), Query.MetricKind, EBlueprintHelperMetricsMetricKind::ToolUsage);
	TestTrue(TEXT("default selected bucket is empty"), Query.SelectedBucketId.IsEmpty());

	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	TestEqual(
		TEXT("snapshot default metric kind"),
		Snapshot.Selection.MetricKind,
		EBlueprintHelperMetricsMetricKind::ToolUsage);
	TestEqual(
		TEXT("snapshot default timeline"),
		Snapshot.Selection.TimelineMode,
		EBlueprintHelperMetricsTimelineMode::Daily);
	TestTrue(TEXT("snapshot selected bucket starts empty"), Snapshot.Selection.SelectedBucketId.IsEmpty());
	TestEqual(TEXT("metric option starts empty"), Snapshot.MetricOptions.Num(), 0);
	TestEqual(TEXT("overview bars start empty"), Snapshot.OverviewBars.Num(), 0);
	TestEqual(TEXT("detail bars start empty"), Snapshot.DetailBars.Num(), 0);
	TestEqual(TEXT("operation rows start empty"), Snapshot.OperationUsageRows.Num(), 0);

	const FBlueprintHelperMetricsPanelVisualEvent MetricEvent =
		FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(
			EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestEqual(TEXT("metric event type"), MetricEvent.Type, EBlueprintHelperMetricsVisualEventType::MetricSelected);
	TestEqual(TEXT("metric event kind"), MetricEvent.MetricKind, EBlueprintHelperMetricsMetricKind::OperationUsage);

	const FBlueprintHelperMetricsPanelVisualEvent BucketEvent =
		FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(TEXT("bucket-2026-06-04"));
	TestEqual(TEXT("bucket event type"), BucketEvent.Type, EBlueprintHelperMetricsVisualEventType::OverviewBucketSelected);
	TestEqual(TEXT("bucket event id"), BucketEvent.BucketId, FString(TEXT("bucket-2026-06-04")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsStoreReaderLoadsJsonlTest,
	"BlueprintHelper.UI.MetricsPanel.StoreReader.LoadsJsonl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsStoreReaderLoadsJsonlTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsPanelTestFiles Files(TEXT("LoadsJsonl"));
	if (!Files.WriteEventFile(
			*this,
			TEXT("session-a.jsonl"),
			TEXT("{\"timestamp\":\"2026-06-04T01:02:03Z\",\"event_type\":\"tool\",\"tool_name\":\"read_context\",\"status\":\"success\",\"duration_ms\":25}\n")
			TEXT("{\"timestamp\":\"2026-06-04T01:05:00Z\",\"event_type\":\"tool\",\"tool_name\":\"taskspec_execute\",\"status\":\"success\",\"io\":{\"input_source\":\"chat\",\"input_chars\":1148,\"input_utf8_bytes\":1148,\"output_chars\":512,\"output_utf8_bytes\":512,\"estimated_input_tokens\":250,\"estimated_output_tokens\":100},\"duration_ms\":400}\n")))
	{
		return false;
	}

	const FBlueprintHelperMetricsLoadResult Result =
		FBlueprintHelperMetricsStoreReader::LoadFromRoot(Files.Root);

	TestTrue(TEXT("load succeeds"), Result.bSucceeded);
	TestEqual(TEXT("metrics root preserved"), Result.MetricsRoot, Files.Root);
	TestEqual(TEXT("reads one file"), Result.FilesRead, 1);
	TestEqual(TEXT("reads two lines"), Result.LinesRead, 2);
	TestEqual(TEXT("skips zero lines"), Result.LinesSkipped, 0);
	TestEqual(TEXT("parse warnings remain zero"), Result.ParseWarnings, 0);
	TestEqual(TEXT("loads two events"), Result.Events.Num(), 2);
	if (Result.Events.Num() >= 2)
	{
		TestEqual(TEXT("first tool name"), Result.Events[0].ToolName, FString(TEXT("read_context")));
		TestTrue(TEXT("second event has io"), Result.Events[1].bHasIo);
		TestEqual(TEXT("second event input chars"), Result.Events[1].Io.InputChars, static_cast<int64>(1148));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsStoreReaderSkipsMalformedJsonlTest,
	"BlueprintHelper.UI.MetricsPanel.StoreReader.SkipsMalformedJsonl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsStoreReaderSkipsMalformedJsonlTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsPanelTestFiles Files(TEXT("SkipsMalformedJsonl"));
	if (!Files.WriteEventFile(
			*this,
			TEXT("session-b.jsonl"),
			TEXT("{\"timestamp\":\"2026-06-04T02:00:00Z\",\"event_type\":\"tool\",\"tool_name\":\"preview\",\"status\":\"success\"}\n")
			TEXT("{\"timestamp\":\"2026-06-04T02:00:01Z\",\"event_type\":\"tool\",\"tool_name\":\"broken\"\n")
			TEXT("{\"timestamp\":\"2026-06-04T02:00:02Z\",\"event_type\":\"tool\",\"tool_name\":\"bad_numeric\",\"status\":\"success\",\"duration_ms\":\"slow\"}\n")))
	{
		return false;
	}

	const FBlueprintHelperMetricsLoadResult Result =
		FBlueprintHelperMetricsStoreReader::LoadFromRoot(Files.Root);

	TestTrue(TEXT("load succeeds despite malformed line"), Result.bSucceeded);
	TestEqual(TEXT("loads one valid event"), Result.Events.Num(), 1);
	TestEqual(TEXT("files read"), Result.FilesRead, 1);
	TestEqual(TEXT("lines read"), Result.LinesRead, 3);
	TestEqual(TEXT("two lines skipped"), Result.LinesSkipped, 2);
	TestEqual(TEXT("two parse warnings"), Result.ParseWarnings, 2);
	if (Result.Events.Num() == 1)
	{
		TestEqual(TEXT("valid event tool name"), Result.Events[0].ToolName, FString(TEXT("preview")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsStoreReaderLoadsDefaultFromEnvTest,
	"BlueprintHelper.UI.MetricsPanel.StoreReader.LoadsDefaultFromEnv",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsStoreReaderLoadsDefaultFromEnvTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsPanelTestFiles Files(TEXT("LoadsDefaultFromEnv"));
	if (!Files.WriteEventFile(
			*this,
			TEXT("session-env.jsonl"),
			TEXT("{\"timestamp\":\"2026-06-04T03:00:00Z\",\"event_type\":\"tool\",\"tool_name\":\"env_reader\",\"status\":\"success\"}\n")))
	{
		return false;
	}

	const FBlueprintHelperMetricsScopedEnvVar EnvOverride(TEXT("BPH_METRICS_DIR"), Files.Root);
	const FString ExpectedRoot = FPaths::ConvertRelativePathToFull(Files.Root);
	TestEqual(
		TEXT("env root resolves"),
		FBlueprintHelperMetricsStoreReader::ResolveMetricsRootFromEnvironment(),
		ExpectedRoot);

	const FBlueprintHelperMetricsLoadResult Result =
		FBlueprintHelperMetricsStoreReader::LoadDefault();

	TestTrue(TEXT("load default succeeds"), Result.bSucceeded);
	TestEqual(TEXT("default root comes from env"), Result.MetricsRoot, ExpectedRoot);
	TestEqual(TEXT("loads one env event"), Result.Events.Num(), 1);
	if (Result.Events.Num() == 1)
	{
		TestEqual(TEXT("env event tool name"), Result.Events[0].ToolName, FString(TEXT("env_reader")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesBuildsDailyBucketsTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.BuildsDailyBuckets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesBuildsDailyBucketsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;

	FBlueprintHelperMetricsEvent ExecuteEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-03T09:15:00Z"),
		TEXT("taskspec_execute_completed"),
		TEXT("blueprinthelper_execute_task"),
		TEXT("success"));
	BlueprintHelperMetricsSetTaskKey(
		ExecuteEvent,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsPanel"),
		TEXT("blueprint"));
	LoadResult.Events.Add(ExecuteEvent);

	FBlueprintHelperMetricsEvent ToolEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T10:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_read_context"),
		TEXT("success"));
	LoadResult.Events.Add(ToolEvent);

	FBlueprintHelperMetricsEvent PreviewFailureEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T11:00:00Z"),
		TEXT("taskspec_preview_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("failed"));
	BlueprintHelperMetricsSetTaskKey(
		PreviewFailureEvent,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsPanel"),
		TEXT("blueprint"));
	BlueprintHelperMetricsSetFailure(
		PreviewFailureEvent,
		TEXT("parameter_error"),
		TEXT("taskspec_semantic_invalid"),
		TEXT("unconsumed_pure_data_node"));
	LoadResult.Events.Add(PreviewFailureEvent);

	FBlueprintHelperMetricsEvent IoEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T11:05:00Z"),
		TEXT("cli_io_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("success"));
	BlueprintHelperMetricsSetIo(IoEvent, 1200, 1200, 1600, 1600, 300, 400);
	LoadResult.Events.Add(IoEvent);

	LoadResult.Events.Add(BlueprintHelperMetricsMakeEvent(
		TEXT("not-a-timestamp"),
		TEXT("tool_completed"),
		TEXT("ignored_invalid_timestamp"),
		TEXT("success")));

	FBlueprintHelperMetricsQuery Query;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.DailyBucketCount = 3;
	Query.TopRowLimit = 5;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("daily timeline mode"), Snapshot.TimelineMode, EBlueprintHelperMetricsTimelineMode::Daily);
	TestEqual(TEXT("daily bucket count"), Snapshot.Buckets.Num(), 3);
	TestEqual(TEXT("summary total events"), Snapshot.Summary.TotalEvents, 4);
	TestEqual(TEXT("summary total failures"), Snapshot.Summary.TotalFailures, 1);
	TestEqual(TEXT("summary unknown errors"), Snapshot.Summary.UnknownErrors, 0);
	TestEqual(TEXT("summary input tokens"), Snapshot.Summary.EstimatedInputTokens, static_cast<int64>(300));
	TestEqual(TEXT("summary output tokens"), Snapshot.Summary.EstimatedOutputTokens, static_cast<int64>(400));
	TestEqual(TEXT("tool usage row count excludes io"), Snapshot.ToolUsageRows.Num(), 3);
	TestEqual(TEXT("task health rows"), Snapshot.TaskHealthRows.Num(), 1);
	TestEqual(TEXT("error category rows"), Snapshot.ErrorCategoryRows.Num(), 1);
	TestEqual(TEXT("top error rows"), Snapshot.TopErrorRows.Num(), 1);
	TestEqual(TEXT("io usage rows"), Snapshot.IoUsageRows.Num(), 1);

	if (Snapshot.Buckets.Num() == 3)
	{
		TestEqual(
			TEXT("oldest daily bucket start"),
			Snapshot.Buckets[0].StartUtc,
			BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 2));
		TestEqual(
			TEXT("middle daily bucket start"),
			Snapshot.Buckets[1].StartUtc,
			BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 3));
		TestEqual(
			TEXT("latest daily bucket start"),
			Snapshot.Buckets[2].StartUtc,
			BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4));
		TestEqual(TEXT("latest daily bucket label"), Snapshot.Buckets[2].Label, FString(TEXT("2026-06-04")));
		TestEqual(TEXT("middle day total events"), Snapshot.Buckets[1].TotalEvents, 1);
		TestEqual(TEXT("middle day execute attempts"), Snapshot.Buckets[1].TaskSpecExecuteAttempts, 1);
		TestEqual(TEXT("latest day total events"), Snapshot.Buckets[2].TotalEvents, 3);
		TestEqual(TEXT("latest day tool events"), Snapshot.Buckets[2].ToolEvents, 2);
		TestEqual(TEXT("latest day success count"), Snapshot.Buckets[2].SuccessCount, 2);
		TestEqual(TEXT("latest day failure count"), Snapshot.Buckets[2].FailureCount, 1);
		TestEqual(TEXT("latest day preview attempts"), Snapshot.Buckets[2].TaskSpecPreviewAttempts, 1);
		TestEqual(
			TEXT("latest day parameter errors"),
			Snapshot.Buckets[2].ErrorCategoryCounts.FindRef(TEXT("parameter_error")),
			1);
		TestEqual(
			TEXT("latest day top error"),
			Snapshot.Buckets[2].TopErrorCounts.FindRef(TEXT("taskspec_semantic_invalid")),
			1);
	}

	if (Snapshot.TaskHealthRows.Num() == 1)
	{
		TestEqual(TEXT("task health preview"), Snapshot.TaskHealthRows[0].PreviewAttempts, 1);
		TestEqual(TEXT("task health execute"), Snapshot.TaskHealthRows[0].ExecuteAttempts, 1);
		TestEqual(TEXT("task health failed"), Snapshot.TaskHealthRows[0].FailedAttempts, 1);
		TestEqual(TEXT("task health success"), Snapshot.TaskHealthRows[0].SuccessAttempts, 1);
	}

	if (Snapshot.ErrorCategoryRows.Num() == 1)
	{
		TestEqual(TEXT("error category"), Snapshot.ErrorCategoryRows[0].Category, FString(TEXT("parameter_error")));
		TestEqual(TEXT("error category count"), Snapshot.ErrorCategoryRows[0].Count, 1);
	}

	if (Snapshot.TopErrorRows.Num() == 1)
	{
		TestEqual(TEXT("top error code"), Snapshot.TopErrorRows[0].ErrorCode, FString(TEXT("taskspec_semantic_invalid")));
		TestEqual(TEXT("top issue code"), Snapshot.TopErrorRows[0].IssueCode, FString(TEXT("unconsumed_pure_data_node")));
		TestEqual(TEXT("top error count"), Snapshot.TopErrorRows[0].Count, 1);
	}

	if (Snapshot.IoUsageRows.Num() == 1)
	{
		TestEqual(TEXT("io tool name"), Snapshot.IoUsageRows[0].ToolName, FString(TEXT("blueprinthelper_preview_task")));
		TestEqual(TEXT("io row total"), Snapshot.IoUsageRows[0].Total, 1);
		TestEqual(TEXT("io row input chars"), Snapshot.IoUsageRows[0].InputChars, static_cast<int64>(1200));
		TestEqual(TEXT("io row output chars"), Snapshot.IoUsageRows[0].OutputChars, static_cast<int64>(1600));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesBuildsMetricKindProjectionTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.BuildsMetricKindProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesBuildsMetricKindProjectionTest::RunTest(
	const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;

	FBlueprintHelperMetricsEvent ReadEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_read_context"),
		TEXT("success"));
	BlueprintHelperMetricsSetOperation(ReadEvent, TEXT("read_context"), TEXT("logic_flow"));
	LoadResult.Events.Add(ReadEvent);

	FBlueprintHelperMetricsEvent PreviewFailure = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T10:00:00Z"),
		TEXT("taskspec_preview_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("failed"));
	BlueprintHelperMetricsSetTaskKey(
		PreviewFailure,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsPanelABC"),
		TEXT("blueprint"));
	BlueprintHelperMetricsSetFailure(
		PreviewFailure,
		TEXT("parameter_error"),
		TEXT("taskspec_semantic_invalid"),
		TEXT("unconsumed_pure_data_node"));
	BlueprintHelperMetricsSetOperation(PreviewFailure, TEXT("graph_write"), TEXT("append_after"));
	LoadResult.Events.Add(PreviewFailure);

	FBlueprintHelperMetricsEvent IoEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T10:01:00Z"),
		TEXT("cli_io_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("success"));
	BlueprintHelperMetricsSetIo(IoEvent, 1200, 1200, 1600, 1600, 300, 400);
	LoadResult.Events.Add(IoEvent);

	FBlueprintHelperMetricsQuery Query;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.DailyBucketCount = 2;
	Query.TopRowLimit = 5;
	Query.MetricKind = EBlueprintHelperMetricsMetricKind::OperationUsage;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("selection metric"), Snapshot.Selection.MetricKind, EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestEqual(TEXT("metric options count"), Snapshot.MetricOptions.Num(), 7);
	TestEqual(TEXT("overview bars count"), Snapshot.OverviewBars.Num(), 2);
	TestFalse(TEXT("selected bucket id is assigned"), Snapshot.Selection.SelectedBucketId.IsEmpty());
	TestEqual(TEXT("operation rows count"), Snapshot.OperationUsageRows.Num(), 2);
	TestTrue(TEXT("detail bars exist"), Snapshot.DetailBars.Num() >= 2);

	if (Snapshot.DetailBars.Num() >= 1)
	{
		TestTrue(
			TEXT("detail label includes operation identity"),
			Snapshot.DetailBars[0].Label.Contains(TEXT(" / ")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesMetricSelectionChangesOverviewProjectionTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.MetricSelectionChangesOverviewProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesMetricSelectionChangesOverviewProjectionTest::RunTest(
	const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;

	FBlueprintHelperMetricsEvent ReadToolEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_read_context"),
		TEXT("success"));
	LoadResult.Events.Add(ReadToolEvent);

	FBlueprintHelperMetricsEvent ExecuteAttempt = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:10:00Z"),
		TEXT("taskspec_execute_completed"),
		TEXT("blueprinthelper_execute_task"),
		TEXT("success"));
	BlueprintHelperMetricsSetTaskKey(
		ExecuteAttempt,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsProjection"),
		TEXT("blueprint"));
	LoadResult.Events.Add(ExecuteAttempt);

	FBlueprintHelperMetricsEvent AnotherExecuteAttempt = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:20:00Z"),
		TEXT("taskspec_execute_completed"),
		TEXT("blueprinthelper_execute_task"),
		TEXT("failed"));
	BlueprintHelperMetricsSetTaskKey(
		AnotherExecuteAttempt,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsProjection"),
		TEXT("blueprint"));
	LoadResult.Events.Add(AnotherExecuteAttempt);

	FBlueprintHelperMetricsEvent EarlierToolEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-03T08:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("success"));
	LoadResult.Events.Add(EarlierToolEvent);

	FBlueprintHelperMetricsQuery AttemptsQuery;
	AttemptsQuery.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	AttemptsQuery.DailyBucketCount = 2;
	AttemptsQuery.MetricKind = EBlueprintHelperMetricsMetricKind::TaskSpecAttempts;

	FBlueprintHelperMetricsQuery ToolQuery = AttemptsQuery;
	ToolQuery.MetricKind = EBlueprintHelperMetricsMetricKind::ToolUsage;

	const FBlueprintHelperMetricsPanelSnapshot AttemptSnapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, AttemptsQuery);
	const FBlueprintHelperMetricsPanelSnapshot ToolSnapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, ToolQuery);

	TestEqual(
		TEXT("attempt snapshot uses requested metric"),
		AttemptSnapshot.Selection.MetricKind,
		EBlueprintHelperMetricsMetricKind::TaskSpecAttempts);
	TestEqual(
		TEXT("tool snapshot uses requested metric"),
		ToolSnapshot.Selection.MetricKind,
		EBlueprintHelperMetricsMetricKind::ToolUsage);
	TestTrue(
		TEXT("projection snapshots have latest bars"),
		AttemptSnapshot.OverviewBars.Num() > 0 && ToolSnapshot.OverviewBars.Num() > 0);
	if (AttemptSnapshot.OverviewBars.Num() > 0 && ToolSnapshot.OverviewBars.Num() > 0)
	{
		const FBlueprintHelperMetricsOverviewBarView& AttemptBar =
			AttemptSnapshot.OverviewBars.Last();
		const FBlueprintHelperMetricsOverviewBarView& ToolBar =
			ToolSnapshot.OverviewBars.Last();
		TestEqual(TEXT("attempt metric latest total"), AttemptBar.Value, static_cast<int64>(2));
		TestEqual(TEXT("tool metric latest total"), ToolBar.Value, static_cast<int64>(3));
		TestNotEqual(TEXT("latest bar values differ by metric"), AttemptBar.Value, ToolBar.Value);
		if (AttemptSnapshot.OverviewBars.Num() > 1 && ToolSnapshot.OverviewBars.Num() > 1)
		{
			TestNotEqual(
				TEXT("earlier bar fractions differ by metric"),
				AttemptSnapshot.OverviewBars[0].Fraction,
				ToolSnapshot.OverviewBars[0].Fraction);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesBuildsBucketSpecificDetailsTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.BuildsBucketSpecificDetails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesBuildsBucketSpecificDetailsTest::RunTest(
	const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;

	LoadResult.Events.Add(BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-03T09:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_read_context"),
		TEXT("success")));

	FBlueprintHelperMetricsEvent TodayTool = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:00:00Z"),
		TEXT("tool_completed"),
		TEXT("blueprinthelper_execute_task"),
		TEXT("success"));
	LoadResult.Events.Add(TodayTool);

	FBlueprintHelperMetricsQuery Query;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.DailyBucketCount = 2;
	Query.MetricKind = EBlueprintHelperMetricsMetricKind::ToolUsage;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("selected bucket label"), Snapshot.SelectedBucketLabel, FString(TEXT("2026-06-04")));
	TestEqual(TEXT("selected bucket total"), Snapshot.SelectedBucketTotal, static_cast<int64>(1));
	TestEqual(TEXT("detail bars count"), Snapshot.DetailBars.Num(), 1);
	if (Snapshot.DetailBars.Num() == 1)
	{
		TestEqual(TEXT("today tool detail"), Snapshot.DetailBars[0].Label, FString(TEXT("blueprinthelper_execute_task")));
		TestEqual(TEXT("today tool value"), Snapshot.DetailBars[0].Value, static_cast<int64>(1));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesSeparatesIoFromToolUsageTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.SeparatesIoFromToolUsage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesSeparatesIoFromToolUsageTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;
	LoadResult.Events.Add(BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T07:00:00Z"),
		TEXT("tool_completed"),
		TEXT("tool_alpha"),
		TEXT("success")));

	FBlueprintHelperMetricsEvent ToolFailureEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T08:00:00Z"),
		TEXT("tool_completed"),
		TEXT("tool_beta"),
		TEXT("failed"));
	BlueprintHelperMetricsSetFailure(
		ToolFailureEvent,
		TEXT("runtime_error"),
		TEXT("tool_failed"),
		TEXT("tool_failure"));
	LoadResult.Events.Add(ToolFailureEvent);

	FBlueprintHelperMetricsEvent IoEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T08:05:00Z"),
		TEXT("cli_io_completed"),
		TEXT("tool_alpha"),
		TEXT("success"));
	BlueprintHelperMetricsSetIo(IoEvent, 256, 300, 1024, 1100, 64, 256);
	LoadResult.Events.Add(IoEvent);

	FBlueprintHelperMetricsQuery Query;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.DailyBucketCount = 1;
	Query.TopRowLimit = 4;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("summary total events includes io"), Snapshot.Summary.TotalEvents, 3);
	TestEqual(TEXT("summary failures"), Snapshot.Summary.TotalFailures, 1);
	TestEqual(TEXT("tool usage rows exclude io event"), Snapshot.ToolUsageRows.Num(), 2);
	TestEqual(TEXT("io rows include io event"), Snapshot.IoUsageRows.Num(), 1);

	if (Snapshot.Buckets.Num() == 1)
	{
		TestEqual(TEXT("bucket total events includes io"), Snapshot.Buckets[0].TotalEvents, 3);
		TestEqual(TEXT("bucket tool events exclude io"), Snapshot.Buckets[0].ToolEvents, 2);
	}

	if (Snapshot.ToolUsageRows.Num() == 2)
	{
		TestEqual(TEXT("top tool row is tool_alpha"), Snapshot.ToolUsageRows[0].Name, FString(TEXT("tool_alpha")));
		TestEqual(TEXT("top tool row total"), Snapshot.ToolUsageRows[0].Total, 1);
		TestEqual(TEXT("second tool row is tool_beta"), Snapshot.ToolUsageRows[1].Name, FString(TEXT("tool_beta")));
		TestEqual(TEXT("second tool row failed"), Snapshot.ToolUsageRows[1].Failed, 1);
	}

	if (Snapshot.IoUsageRows.Num() == 1)
	{
		TestEqual(TEXT("io row keeps tool name"), Snapshot.IoUsageRows[0].ToolName, FString(TEXT("tool_alpha")));
		TestEqual(TEXT("io row total"), Snapshot.IoUsageRows[0].Total, 1);
		TestEqual(TEXT("io row input tokens"), Snapshot.IoUsageRows[0].EstimatedInputTokens, static_cast<int64>(64));
		TestEqual(TEXT("io row output tokens"), Snapshot.IoUsageRows[0].EstimatedOutputTokens, static_cast<int64>(256));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesAggregatesTaskHealthVisibleRowsTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.AggregatesTaskHealthVisibleRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesAggregatesTaskHealthVisibleRowsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;

	FBlueprintHelperMetricsEvent FirstEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T09:00:00Z"),
		TEXT("taskspec_preview_completed"),
		TEXT("blueprinthelper_preview_task"),
		TEXT("failed"));
	BlueprintHelperMetricsSetTaskKey(
		FirstEvent,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsPanel"),
		TEXT("blueprint"));
	FirstEvent.TaskKey.TargetRefHash = TEXT("sha256:first");
	FirstEvent.TaskKey.TargetRefLabel = TEXT("/Game/A");
	BlueprintHelperMetricsSetFailure(
		FirstEvent,
		TEXT("parameter_error"),
		TEXT("taskspec_semantic_invalid"),
		TEXT("first_issue"));
	LoadResult.Events.Add(FirstEvent);

	FBlueprintHelperMetricsEvent SecondEvent = BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-04T10:00:00Z"),
		TEXT("taskspec_execute_completed"),
		TEXT("blueprinthelper_execute_task"),
		TEXT("success"));
	BlueprintHelperMetricsSetTaskKey(
		SecondEvent,
		TEXT("edit_blueprint_graph"),
		TEXT("MetricsPanel"),
		TEXT("blueprint"));
	SecondEvent.TaskKey.TargetRefHash = TEXT("sha256:second");
	SecondEvent.TaskKey.TargetRefLabel = TEXT("/Game/B");
	LoadResult.Events.Add(SecondEvent);

	FBlueprintHelperMetricsQuery Query;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.DailyBucketCount = 1;
	Query.TopRowLimit = 5;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("one visible task health row"), Snapshot.TaskHealthRows.Num(), 1);
	if (Snapshot.TaskHealthRows.Num() == 1)
	{
		TestEqual(TEXT("task type"), Snapshot.TaskHealthRows[0].TaskType, FString(TEXT("edit_blueprint_graph")));
		TestEqual(TEXT("feature name"), Snapshot.TaskHealthRows[0].FeatureName, FString(TEXT("MetricsPanel")));
		TestEqual(TEXT("target type"), Snapshot.TaskHealthRows[0].TargetType, FString(TEXT("blueprint")));
		TestEqual(TEXT("merged preview attempts"), Snapshot.TaskHealthRows[0].PreviewAttempts, 1);
		TestEqual(TEXT("merged execute attempts"), Snapshot.TaskHealthRows[0].ExecuteAttempts, 1);
		TestEqual(TEXT("merged failed attempts"), Snapshot.TaskHealthRows[0].FailedAttempts, 1);
		TestEqual(TEXT("merged success attempts"), Snapshot.TaskHealthRows[0].SuccessAttempts, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsTimeSeriesBuildsIsoWeeklyBucketsTest,
	"BlueprintHelper.UI.MetricsPanel.TimeSeries.BuildsIsoWeeklyBuckets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsTimeSeriesBuildsIsoWeeklyBucketsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperMetricsLoadResult LoadResult;
	LoadResult.Events.Add(BlueprintHelperMetricsMakeEvent(
		TEXT("2026-05-31T23:30:00Z"),
		TEXT("tool_completed"),
		TEXT("week_old"),
		TEXT("success")));
	LoadResult.Events.Add(BlueprintHelperMetricsMakeEvent(
		TEXT("2026-06-01T01:00:00Z"),
		TEXT("tool_completed"),
		TEXT("week_new"),
		TEXT("success")));

	FBlueprintHelperMetricsQuery Query;
	Query.TimelineMode = EBlueprintHelperMetricsTimelineMode::Weekly;
	Query.NowUtc = BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 4, 12, 0, 0);
	Query.WeeklyBucketCount = 2;
	Query.TopRowLimit = 3;

	const FBlueprintHelperMetricsPanelSnapshot Snapshot =
		FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);

	TestEqual(TEXT("weekly timeline mode"), Snapshot.TimelineMode, EBlueprintHelperMetricsTimelineMode::Weekly);
	TestEqual(TEXT("weekly bucket count"), Snapshot.Buckets.Num(), 2);
	TestEqual(TEXT("weekly summary total events"), Snapshot.Summary.TotalEvents, 2);

	if (Snapshot.Buckets.Num() == 2)
	{
		TestEqual(
			TEXT("previous iso week start"),
			Snapshot.Buckets[0].StartUtc,
			BlueprintHelperMetricsMakeUtcDateTime(2026, 5, 25));
		TestEqual(
			TEXT("current iso week start"),
			Snapshot.Buckets[1].StartUtc,
			BlueprintHelperMetricsMakeUtcDateTime(2026, 6, 1));
		TestEqual(TEXT("current iso week label"), Snapshot.Buckets[1].Label, FString(TEXT("W 2026-06-01")));
		TestEqual(TEXT("previous iso week event count"), Snapshot.Buckets[0].TotalEvents, 1);
		TestEqual(TEXT("current iso week event count"), Snapshot.Buckets[1].TotalEvents, 1);
		TestEqual(TEXT("previous iso week tool events"), Snapshot.Buckets[0].ToolEvents, 1);
		TestEqual(TEXT("current iso week tool events"), Snapshot.Buckets[1].ToolEvents, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterRefreshRetainsLoadedUiTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.RefreshRetainsLoadedUi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterRefreshRetainsLoadedUiTest::RunTest(
	const FString& Parameters)
{
	std::atomic<int32> LoadCallCount{0};
	std::atomic<bool> bAllowSecondLoadFinish{false};
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;

	const TSharedRef<FBlueprintHelperMetricsPanelPresenter> Presenter =
		MakeShared<FBlueprintHelperMetricsPanelPresenter>(
			[&LoadCallCount, &bAllowSecondLoadFinish](
				const FBlueprintHelperMetricsPanelSelection& Selection)
			{
				const int32 CallIndex = LoadCallCount.fetch_add(1) + 1;
				if (CallIndex == 2)
				{
					const double DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
					while (!bAllowSecondLoadFinish.load() &&
						FPlatformTime::Seconds() < DeadlineSeconds)
					{
						FPlatformProcess::Sleep(0.001f);
					}
				}

				return BlueprintHelperMetricsMakeLoadedPresenterSnapshot(
					Selection,
					CallIndex == 1 ? 12 : 18,
					TEXT("Loaded Metrics"));
			});
	Presenter->SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	PresenterEvents.Reset();

	Presenter->HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());
	TestEqual(TEXT("initial load called once"), LoadCallCount.load(), 1);
	TestEqual(
		TEXT("initial snapshot is loaded"),
		Presenter->GetSnapshot().LoadState,
		EBlueprintHelperMetricsLoadState::Loaded);
	TestEqual(TEXT("initial overview bars exist"), Presenter->GetSnapshot().OverviewBars.Num(), 2);
	TestEqual(TEXT("initial detail bars exist"), Presenter->GetSnapshot().DetailBars.Num(), 1);

	PresenterEvents.Reset();
	Presenter->HandleVisualEvent(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());

	const FBlueprintHelperMetricsPanelSnapshot& RefreshingSnapshot =
		Presenter->GetSnapshot();
	TestEqual(
		TEXT("refresh keeps loaded state"),
		RefreshingSnapshot.LoadState,
		EBlueprintHelperMetricsLoadState::Loaded);
	TestEqual(
		TEXT("refresh keeps previous total"),
		RefreshingSnapshot.Summary.TotalEvents,
		12);
	TestEqual(
		TEXT("refresh keeps overview bars"),
		RefreshingSnapshot.OverviewBars.Num(),
		2);
	TestEqual(
		TEXT("refresh keeps detail bars"),
		RefreshingSnapshot.DetailBars.Num(),
		1);
	TestTrue(
		TEXT("refresh marks progress without replacing content"),
		RefreshingSnapshot.bRefreshInProgress);
	TestTrue(
		TEXT("refresh status text is available"),
		!RefreshingSnapshot.RefreshStatusText.IsEmpty());
	if (PresenterEvents.Num() > 0)
	{
		TestEqual(
			TEXT("refresh progress emits status-only scope"),
			PresenterEvents.Last().UpdateScope,
			EBlueprintHelperMetricsPanelUpdateScope::StatusOnly);
	}

	bAllowSecondLoadFinish.store(true);
	const bool bRefreshCompleted = BlueprintHelperMetricsPumpAsyncUntil(
		[&Presenter]()
		{
			const FBlueprintHelperMetricsPanelSnapshot& Snapshot =
				Presenter->GetSnapshot();
			return Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded &&
				!Snapshot.bRefreshInProgress &&
				Snapshot.Summary.TotalEvents == 18;
		});
	TestTrue(TEXT("refresh eventually completes"), bRefreshCompleted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterSelectionUsesCachedProjectionTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.SelectionUsesCachedProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterSelectionUsesCachedProjectionTest::RunTest(
	const FString& Parameters)
{
	int32 LoadCallCount = 0;
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;

	FBlueprintHelperMetricsPanelPresenter Presenter(
		[&LoadCallCount](const FBlueprintHelperMetricsPanelSelection& Selection)
		{
			++LoadCallCount;
			return BlueprintHelperMetricsMakeLoadedPresenterSnapshot(
				Selection,
				10,
				TEXT("Loaded Metrics"));
		});
	Presenter.SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	PresenterEvents.Reset();

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());
	TestEqual(TEXT("initial load uses loader once"), LoadCallCount, 1);

	PresenterEvents.Reset();
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(
			EBlueprintHelperMetricsMetricKind::OperationUsage));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(
			TEXT("bucket-b")));

	TestEqual(TEXT("selection events reuse cached projection"), LoadCallCount, 1);
	for (const FBlueprintHelperMetricsPanelPresenterEvent& Event : PresenterEvents)
	{
		TestNotEqual(
			TEXT("selection event never emits loading"),
			Event.Snapshot.LoadState,
			EBlueprintHelperMetricsLoadState::Loading);
	}
	TestEqual(
		TEXT("cached metric selection stored"),
		Presenter.GetSnapshot().Selection.MetricKind,
		EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestEqual(
		TEXT("cached bucket selection stored"),
		Presenter.GetSnapshot().Selection.SelectedBucketId,
		FString(TEXT("bucket-b")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterScopesBucketClickTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.ScopesBucketClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterScopesBucketClickTest::RunTest(
	const FString& Parameters)
{
	FBlueprintHelperMetricsPanelPresenter Presenter(
		[](const FBlueprintHelperMetricsPanelSelection& Selection)
		{
			return BlueprintHelperMetricsMakeLoadedPresenterSnapshot(
				Selection,
				10,
				TEXT("Loaded Metrics"));
		});

	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;
	Presenter.SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());
	PresenterEvents.Reset();

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(
			TEXT("bucket-b")));

	TestTrue(TEXT("bucket click emits an event"), PresenterEvents.Num() > 0);
	if (PresenterEvents.Num() > 0)
	{
		const FBlueprintHelperMetricsPanelPresenterEvent& Event =
			PresenterEvents.Last();
		TestEqual(
			TEXT("bucket click updates overview and detail only"),
			Event.UpdateScope,
			EBlueprintHelperMetricsPanelUpdateScope::OverviewAndDetail);
		TestNotEqual(
			TEXT("bucket click does not request initial content"),
			Event.UpdateScope,
			EBlueprintHelperMetricsPanelUpdateScope::InitialContent);
		TestNotEqual(
			TEXT("bucket click does not request all regions"),
			Event.UpdateScope,
			EBlueprintHelperMetricsPanelUpdateScope::AllRegions);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterSwitchesTimelineModeTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.SwitchesTimelineMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterSwitchesTimelineModeTest::RunTest(
	const FString& Parameters)
{
	int32 LoadCallCount = 0;
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;

	FBlueprintHelperMetricsPanelPresenter Presenter(
		[&LoadCallCount](const FBlueprintHelperMetricsPanelSelection& Selection)
		{
			++LoadCallCount;
			return BlueprintHelperMetricsMakePresenterSnapshot(
				Selection.TimelineMode,
				EBlueprintHelperMetricsLoadState::Loaded,
				6,
				TEXT("Loaded Metrics"));
		});
	Presenter.SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	PresenterEvents.Reset();

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
			EBlueprintHelperMetricsTimelineMode::Weekly));

	TestEqual(TEXT("load called once"), LoadCallCount, 1);
	TestTrue(TEXT("presenter emitted events"), PresenterEvents.Num() >= 1);
	TestEqual(
		TEXT("stored timeline switches to weekly"),
		Presenter.GetSnapshot().TimelineMode,
		EBlueprintHelperMetricsTimelineMode::Weekly);
	TestEqual(
		TEXT("stored snapshot is loaded"),
		Presenter.GetSnapshot().LoadState,
		EBlueprintHelperMetricsLoadState::Loaded);
	TestEqual(TEXT("stored total events"), Presenter.GetSnapshot().Summary.TotalEvents, 6);

	if (PresenterEvents.Num() >= 1)
	{
		const FBlueprintHelperMetricsPanelPresenterEvent& Event =
			PresenterEvents.Last();
		TestTrue(TEXT("refresh event requests view refresh"), Event.bRefreshView);
		TestEqual(
			TEXT("emitted timeline is weekly"),
			Event.Snapshot.TimelineMode,
			EBlueprintHelperMetricsTimelineMode::Weekly);
		TestEqual(
			TEXT("emitted snapshot is loaded"),
			Event.Snapshot.LoadState,
			EBlueprintHelperMetricsLoadState::Loaded);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterRoutesSelectionEventsTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.RoutesSelectionEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterRoutesSelectionEventsTest::RunTest(
	const FString& Parameters)
{
	TArray<FBlueprintHelperMetricsPanelSelection> LoadedSelections;
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;

	FBlueprintHelperMetricsPanelPresenter Presenter(
		[&LoadedSelections](const FBlueprintHelperMetricsPanelSelection& Selection)
		{
			LoadedSelections.Add(Selection);
			FBlueprintHelperMetricsPanelSnapshot Snapshot =
				BlueprintHelperMetricsMakePresenterSnapshot(
					Selection.TimelineMode,
					EBlueprintHelperMetricsLoadState::Loaded,
					9,
					TEXT("Loaded Metrics"));
			Snapshot.Selection = Selection;
			return Snapshot;
		});
	Presenter.SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(
			EBlueprintHelperMetricsMetricKind::OperationUsage));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(
			TEXT("selected-bucket")));

	TestEqual(TEXT("selection events load only the initial projection"), LoadedSelections.Num(), 1);
	TestEqual(
		TEXT("first selection metric"),
		LoadedSelections[0].MetricKind,
		EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestTrue(TEXT("metric change clears selected bucket"), LoadedSelections[0].SelectedBucketId.IsEmpty());
	TestEqual(
		TEXT("presenter stores selected bucket"),
		Presenter.GetSnapshot().Selection.SelectedBucketId,
		FString(TEXT("selected-bucket")));
	TestTrue(TEXT("presenter emits refresh events"), PresenterEvents.Num() >= 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterRunsAbcInteractionSequenceTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.RunsAbcInteractionSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterRunsAbcInteractionSequenceTest::RunTest(
	const FString& Parameters)
{
	TArray<FBlueprintHelperMetricsPanelSelection> LoadedSelections;

	FBlueprintHelperMetricsPanelPresenter Presenter(
		[&LoadedSelections](const FBlueprintHelperMetricsPanelSelection& Selection)
		{
			LoadedSelections.Add(Selection);

			FBlueprintHelperMetricsPanelSnapshot Snapshot =
				BlueprintHelperMetricsMakePresenterSnapshot(
					Selection.TimelineMode,
					EBlueprintHelperMetricsLoadState::Loaded,
					12,
					TEXT("Loaded Metrics"));
			Snapshot.Selection = Selection;
			Snapshot.SelectedMetricTitle = TEXT("Operations");
			Snapshot.SelectedBucketLabel = Selection.SelectedBucketId.IsEmpty()
				? TEXT("latest")
				: Selection.SelectedBucketId;
			Snapshot.SelectedBucketTotal = Selection.SelectedBucketId.IsEmpty() ? 0 : 2;

			FBlueprintHelperMetricsDetailBarView Detail;
			Detail.Label = Selection.SelectedBucketId.IsEmpty()
				? TEXT("no bucket")
				: TEXT("graph_write / append_after");
			Detail.Value = Snapshot.SelectedBucketTotal;
			Detail.UnitLabel = TEXT("events");
			Snapshot.DetailBars.Add(Detail);
			return Snapshot;
		});

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(
			EBlueprintHelperMetricsMetricKind::OperationUsage));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(
			TEXT("bucket-operations")));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
			EBlueprintHelperMetricsTimelineMode::Weekly));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
			EBlueprintHelperMetricsTimelineMode::Daily));
	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());

	TestEqual(TEXT("sequence loader call count"), LoadedSelections.Num(), 2);
	TestEqual(
		TEXT("operations selected"),
		LoadedSelections[0].MetricKind,
		EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestEqual(
		TEXT("refresh preserves latest metric"),
		LoadedSelections[1].MetricKind,
		EBlueprintHelperMetricsMetricKind::OperationUsage);
	TestEqual(
		TEXT("refresh uses latest timeline"),
		LoadedSelections[1].TimelineMode,
		EBlueprintHelperMetricsTimelineMode::Daily);
	TestTrue(
		TEXT("timeline changes clear bucket before refresh"),
		LoadedSelections[1].SelectedBucketId.IsEmpty());
	TestEqual(
		TEXT("final detail row exists"),
		Presenter.GetSnapshot().DetailBars.Num(),
		1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterReplaysLatestAsyncRequestTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.ReplaysLatestAsyncRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterReplaysLatestAsyncRequestTest::RunTest(
	const FString& Parameters)
{
	std::atomic<int32> LoadCallCount{0};
	std::atomic<bool> bAllowFirstLoadFinish{false};
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;

	const TSharedRef<FBlueprintHelperMetricsPanelPresenter> Presenter =
		MakeShared<FBlueprintHelperMetricsPanelPresenter>(
			[&LoadCallCount, &bAllowFirstLoadFinish](
				const FBlueprintHelperMetricsPanelSelection& Selection)
			{
				const int32 CallIndex = LoadCallCount.fetch_add(1) + 1;
				if (CallIndex == 1)
				{
					const double DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
					while (!bAllowFirstLoadFinish.load() &&
						FPlatformTime::Seconds() < DeadlineSeconds)
					{
						FPlatformProcess::Sleep(0.001f);
					}
				}

				const int32 TotalEvents =
					Selection.TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
						? 2
						: 1;
				FBlueprintHelperMetricsPanelSnapshot Snapshot =
					BlueprintHelperMetricsMakePresenterSnapshot(
						Selection.TimelineMode,
						EBlueprintHelperMetricsLoadState::Loaded,
						TotalEvents,
						TEXT("Loaded Metrics"));
				Snapshot.Selection = Selection;
				return Snapshot;
			});
	Presenter->SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	PresenterEvents.Reset();

	Presenter->HandleVisualEvent(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());

	const double FirstLoadDeadline = FPlatformTime::Seconds() + 5.0;
	while (LoadCallCount.load() < 1 &&
		FPlatformTime::Seconds() < FirstLoadDeadline)
	{
		FPlatformProcess::Sleep(0.001f);
	}

	const bool bFirstLoadStarted = LoadCallCount.load() >= 1;
	TestTrue(TEXT("first async load starts"), bFirstLoadStarted);
	if (!bFirstLoadStarted)
	{
		bAllowFirstLoadFinish.store(true);
		BlueprintHelperMetricsPumpAsyncUntil([]() { return true; }, 1.0);
		return false;
	}

	Presenter->HandleVisualEvent(
		FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
			EBlueprintHelperMetricsTimelineMode::Weekly));
	TestEqual(
		TEXT("queued request is visible"),
		Presenter->GetSnapshot().TimelineMode,
		EBlueprintHelperMetricsTimelineMode::Weekly);
	TestEqual(
		TEXT("queued request keeps loading state"),
		Presenter->GetSnapshot().LoadState,
		EBlueprintHelperMetricsLoadState::Loading);

	bAllowFirstLoadFinish.store(true);
	const bool bLoadedLatestRequest = BlueprintHelperMetricsPumpAsyncUntil(
		[&Presenter]()
		{
			const FBlueprintHelperMetricsPanelSnapshot& Snapshot =
				Presenter->GetSnapshot();
			return Snapshot.TimelineMode ==
					EBlueprintHelperMetricsTimelineMode::Weekly &&
				Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded &&
				Snapshot.Summary.TotalEvents == 1;
		});

	TestTrue(TEXT("latest queued request is loaded"), bLoadedLatestRequest);
	TestEqual(TEXT("load called once"), LoadCallCount.load(), 1);
	TestTrue(TEXT("presenter emitted async events"), PresenterEvents.Num() >= 2);
	if (PresenterEvents.Num() >= 1)
	{
		const FBlueprintHelperMetricsPanelPresenterEvent& Event =
			PresenterEvents.Last();
		TestEqual(
			TEXT("last async event timeline"),
			Event.Snapshot.TimelineMode,
			EBlueprintHelperMetricsTimelineMode::Weekly);
		TestEqual(
			TEXT("last async event total"),
			Event.Snapshot.Summary.TotalEvents,
			1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelPresenterMissingLoaderEmitsErrorTest,
	"BlueprintHelper.UI.MetricsPanel.Presenter.MissingLoaderEmitsError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelPresenterMissingLoaderEmitsErrorTest::RunTest(
	const FString& Parameters)
{
	TArray<FBlueprintHelperMetricsPanelPresenterEvent> PresenterEvents;
	FBlueprintHelperMetricsPanelPresenter Presenter{
		FBlueprintHelperMetricsPanelPresenter::FLoadSnapshot()};
	Presenter.SetEventSink(
		[&PresenterEvents](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			PresenterEvents.Add(Event);
		});
	PresenterEvents.Reset();

	Presenter.HandleVisualEventForTests(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());

	TestEqual(
		TEXT("missing loader yields error"),
		Presenter.GetSnapshot().LoadState,
		EBlueprintHelperMetricsLoadState::Error);
	TestTrue(
		TEXT("missing loader records error text"),
		Presenter.GetSnapshot().ErrorText.Contains(TEXT("not configured")));
	TestTrue(TEXT("presenter emitted error event"), PresenterEvents.Num() >= 1);
	if (PresenterEvents.Num() >= 1)
	{
		const FBlueprintHelperMetricsPanelPresenterEvent& Event =
			PresenterEvents.Last();
		TestTrue(TEXT("error event requests view refresh"), Event.bRefreshView);
		TestEqual(
			TEXT("error event load state"),
			Event.Snapshot.LoadState,
			EBlueprintHelperMetricsLoadState::Error);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelSlateAbcComponentsConstructTest,
	"BlueprintHelper.UI.MetricsPanel.Slate.AbcComponentsConstruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelSlateAbcComponentsConstructTest::RunTest(
	const FString& Parameters)
{
	TArray<FBlueprintHelperMetricsMetricOptionView> Options;
	FBlueprintHelperMetricsMetricOptionView Option;
	Option.Kind = EBlueprintHelperMetricsMetricKind::ToolUsage;
	Option.Label = TEXT("Tool Calls");
	Option.UnitLabel = TEXT("events");
	Option.Total = 3;
	Option.bIsSelected = true;
	Options.Add(Option);

	TArray<FBlueprintHelperMetricsOverviewBarView> Bars;
	FBlueprintHelperMetricsOverviewBarView Bar;
	Bar.BucketId = TEXT("bucket");
	Bar.Label = TEXT("2026-06-04");
	Bar.Value = 3;
	Bar.Fraction = 1.0f;
	Bar.ValueLabel = TEXT("3");
	Bar.Detail = TEXT("success=3 failed=0");
	Bar.bIsSelected = true;
	Bars.Add(Bar);

	TArray<FBlueprintHelperMetricsDetailBarView> Details;
	FBlueprintHelperMetricsDetailBarView Detail;
	Detail.Label = TEXT("blueprinthelper_read_context");
	Detail.Value = 3;
	Detail.UnitLabel = TEXT("events");
	Detail.SubText = TEXT("supported capability");
	Detail.Fraction = 1.0f;
	Details.Add(Detail);

	FBlueprintHelperMetricsSummary Summary;
	Summary.TotalEvents = 3;
	Summary.TotalFailures = 1;
	Summary.UnknownErrors = 0;
	Summary.EstimatedInputTokens = 12;
	Summary.EstimatedOutputTokens = 34;

	TSharedRef<SWidget> Selector =
		SNew(SBlueprintHelperMetricsMetricSelector)
		.Options(Options)
		.OnMetricSelected(FOnBlueprintHelperMetricsMetricSelected::CreateLambda(
			[](EBlueprintHelperMetricsMetricKind) {}));
	const TSharedRef<SBlueprintHelperMetricsOverviewChart> Overview =
		SNew(SBlueprintHelperMetricsOverviewChart)
		.Title(TEXT("TaskSpec Attempts by Day"))
		.Subtitle(TEXT("Last 14 local days"))
		.TimelineMode(EBlueprintHelperMetricsTimelineMode::Daily)
		.Summary(Summary)
		.Bars(Bars)
		.bRefreshInProgress(true)
		.OnTimelineModeSelected(FOnBlueprintHelperMetricsTimelineModeSelected::CreateLambda(
			[](EBlueprintHelperMetricsTimelineMode) {}))
		.OnBucketSelected(FOnBlueprintHelperMetricsBucketSelected::CreateLambda(
			[](const FString&) {}))
		.OnRefreshClicked(FOnBlueprintHelperMetricsRefreshClicked::CreateLambda(
			[]() {}));
	const TSharedRef<SBlueprintHelperMetricsDetailChart> DetailChart =
		SNew(SBlueprintHelperMetricsDetailChart)
		.Title(TEXT("Tool Calls"))
		.Subtitle(TEXT("2026-06-04"))
		.TotalText(TEXT("3 events"))
		.Rows(Details);

	Overview->SetData(
		TEXT("Operations by Week"),
		TEXT("Last 8 ISO weeks"),
		EBlueprintHelperMetricsTimelineMode::Weekly,
		Summary,
		Bars,
		false);
	DetailChart->SetData(
		TEXT("Operations"),
		TEXT("2026-W23"),
		TEXT("3 events"),
		Details);

	TestTrue(TEXT("selector constructed"), Selector->GetTypeAsString().Len() > 0);
	TestTrue(TEXT("overview constructed"), Overview->GetTypeAsString().Len() > 0);
	TestTrue(TEXT("detail constructed"), DetailChart->GetTypeAsString().Len() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelSlateDetailChartSupportsLargeUpdatesTest,
	"BlueprintHelper.UI.MetricsPanel.Slate.DetailChartSupportsLargeUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelSlateDetailChartSupportsLargeUpdatesTest::RunTest(
	const FString& Parameters)
{
	TArray<FBlueprintHelperMetricsDetailBarView> Rows;
	for (int32 Index = 0; Index < 25; ++Index)
	{
		FBlueprintHelperMetricsDetailBarView Row;
		Row.Label = FString::Printf(TEXT("row-%02d"), Index);
		Row.Value = Index + 1;
		Row.UnitLabel = TEXT("events");
		Row.SubText = FString::Printf(TEXT("subtext-%02d"), Index);
		Row.Fraction = static_cast<float>(Index + 1) / 25.0f;
		Rows.Add(Row);
	}

	const TSharedRef<SBlueprintHelperMetricsDetailChart> DetailChart =
		SNew(SBlueprintHelperMetricsDetailChart)
		.Title(TEXT("Tool Calls"))
		.Subtitle(TEXT("2026-06-04"))
		.TotalText(TEXT("25 events"))
		.Rows(Rows);

	FBlueprintHelperMetricsDetailBarView UpdatedRow = Rows[0];
	UpdatedRow.Value = 3;
	UpdatedRow.SubText = TEXT("unsupported capability/policy");
	Rows[0] = UpdatedRow;

	DetailChart->SetData(
		TEXT("Tool Calls"),
		TEXT("2026-06-04"),
		TEXT("25 events"),
		Rows);

	TestTrue(TEXT("detail chart remains constructed"), DetailChart->GetTypeAsString().Len() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMetricsPanelSlateConstructsTest,
	"BlueprintHelper.UI.MetricsPanel.Slate.Constructs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMetricsPanelSlateConstructsTest::RunTest(
	const FString& Parameters)
{
	TSharedRef<SWidget> Panel = SNew(SBlueprintHelperMetricsPanel);
	TestTrue(TEXT("panel constructed"), Panel.Get().GetVisibility().IsVisible());
	TestEqual(TEXT("panel widget type"), Panel->GetTypeAsString(), FString(TEXT("SBlueprintHelperMetricsPanel")));

	const bool bAsyncTasksFlushed =
		BlueprintHelperMetricsPumpAsyncUntil([]() { return true; }, 1.0);
	TestTrue(TEXT("panel async refresh flushed"), bAsyncTasksFlushed);
	return true;
}

#endif
