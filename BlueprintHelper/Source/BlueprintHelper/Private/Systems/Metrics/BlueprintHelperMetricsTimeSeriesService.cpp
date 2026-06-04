// BlueprintHelper Metrics time-series projection service implementation.

#include "Systems/Metrics/BlueprintHelperMetricsTimeSeriesService.h"

struct FBlueprintHelperMetricsTaskHealthAggregate
{
	FString TaskType;
	FString FeatureName;
	FString TargetType;
	int32 PreviewAttempts = 0;
	int32 ExecuteAttempts = 0;
	int32 FailedAttempts = 0;
	int32 SuccessAttempts = 0;
};

struct FBlueprintHelperMetricsUsageAggregate
{
	int32 Total = 0;
	int32 Success = 0;
	int32 Failed = 0;
};

struct FBlueprintHelperMetricsIoAggregate
{
	FString ToolName;
	int32 Total = 0;
	int64 InputChars = 0;
	int64 OutputChars = 0;
	int64 EstimatedInputTokens = 0;
	int64 EstimatedOutputTokens = 0;
};

struct FBlueprintHelperMetricsErrorAggregate
{
	FString Category;
	FString ErrorCode;
	FString IssueCode;
	int32 Count = 0;
};

static FDateTime BlueprintHelperMetricsStartOfUtcDay(const FDateTime& Value)
{
	return FDateTime(Value.GetYear(), Value.GetMonth(), Value.GetDay());
}

static FDateTime BlueprintHelperMetricsResolveNowUtc(const FBlueprintHelperMetricsQuery& Query)
{
	return Query.NowUtc == FDateTime()
		? FDateTime::UtcNow()
		: Query.NowUtc;
}

static int32 BlueprintHelperMetricsResolveBucketCount(
	const FBlueprintHelperMetricsQuery& Query)
{
	return FMath::Max(
		Query.TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
			? Query.WeeklyBucketCount
			: Query.DailyBucketCount,
		1);
}

static int32 BlueprintHelperMetricsResolveStepDays(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly ? 7 : 1;
}

static int32 BlueprintHelperMetricsResolveTopRowLimit(
	const FBlueprintHelperMetricsQuery& Query)
{
	return FMath::Max(Query.TopRowLimit, 1);
}

static bool BlueprintHelperMetricsTryParseTimestamp(
	const FString& Timestamp,
	FDateTime& OutTimestampUtc)
{
	if (Timestamp.IsEmpty())
	{
		return false;
	}

	return FDateTime::ParseIso8601(*Timestamp, OutTimestampUtc);
}

static bool BlueprintHelperMetricsIsIoEvent(
	const FBlueprintHelperMetricsEvent& Event)
{
	return Event.EventType.Equals(TEXT("cli_io_completed"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperMetricsIsSuccessStatus(const FString& Status)
{
	return Status.Equals(TEXT("success"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperMetricsIsFailureStatus(const FString& Status)
{
	return Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase) ||
		Status.Equals(TEXT("error"), ESearchCase::IgnoreCase) ||
		Status.Equals(TEXT("fatal"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperMetricsIsTaskSpecPreviewEvent(
	const FBlueprintHelperMetricsEvent& Event)
{
	return Event.EventType.Contains(TEXT("taskspec_preview"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperMetricsIsTaskSpecExecuteEvent(
	const FBlueprintHelperMetricsEvent& Event)
{
	return Event.EventType.Contains(TEXT("taskspec_execute"), ESearchCase::IgnoreCase);
}

static FString BlueprintHelperMetricsNormalizeToolName(const FString& ToolName)
{
	return ToolName.IsEmpty() ? TEXT("(unknown)") : ToolName;
}

static FString BlueprintHelperMetricsResolveErrorCategory(
	const FBlueprintHelperMetricsEvent& Event)
{
	return Event.ErrorCategory.IsEmpty() ? TEXT("unknown") : Event.ErrorCategory;
}

static FString BlueprintHelperMetricsResolveTopErrorKey(
	const FBlueprintHelperMetricsEvent& Event)
{
	if (!Event.ErrorCode.IsEmpty())
	{
		return Event.ErrorCode;
	}
	if (Event.bHasIssue && !Event.Issue.Code.IsEmpty())
	{
		return Event.Issue.Code;
	}
	if (!Event.ErrorCategory.IsEmpty())
	{
		return Event.ErrorCategory;
	}
	return TEXT("unknown");
}

static FString BlueprintHelperMetricsMakeTaskHealthKey(
	const FBlueprintHelperMetricsEvent& Event)
{
	return FString::Printf(
		TEXT("%s|%s|%s"),
		*Event.TaskKey.TaskType,
		*Event.TaskKey.FeatureName,
		*Event.TaskKey.TargetType);
}

static FString BlueprintHelperMetricsMakeTopErrorRowKey(
	const FBlueprintHelperMetricsEvent& Event)
{
	return FString::Printf(
		TEXT("%s|%s|%s"),
		*BlueprintHelperMetricsResolveErrorCategory(Event),
		*Event.ErrorCode,
		*(Event.bHasIssue ? Event.Issue.Code : FString()));
}

static FDateTime BlueprintHelperMetricsResolveCurrentBucketStartUtc(
	const FDateTime& NowUtc,
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	const FDateTime StartOfDayUtc = BlueprintHelperMetricsStartOfUtcDay(NowUtc);
	if (TimelineMode == EBlueprintHelperMetricsTimelineMode::Daily)
	{
		return StartOfDayUtc;
	}

	const int32 DaysFromMonday = static_cast<int32>(StartOfDayUtc.GetDayOfWeek());
	return StartOfDayUtc - FTimespan::FromDays(DaysFromMonday);
}

static int32 BlueprintHelperMetricsFindBucketIndex(
	const FDateTime& TimestampUtc,
	const FDateTime& FirstBucketStartUtc,
	int32 BucketCount,
	int32 StepDays)
{
	const FTimespan Delta = TimestampUtc - FirstBucketStartUtc;
	if (Delta.GetTicks() < 0)
	{
		return INDEX_NONE;
	}

	const int32 WholeDays = Delta.GetDays();
	const int32 BucketIndex = WholeDays / StepDays;
	return BucketIndex >= 0 && BucketIndex < BucketCount
		? BucketIndex
		: INDEX_NONE;
}

static FString BlueprintHelperMetricsMakeBucketLabel(
	const FDateTime& StartUtc,
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? FString::Printf(
			TEXT("W %04d-%02d-%02d"),
			StartUtc.GetYear(),
			StartUtc.GetMonth(),
			StartUtc.GetDay())
		: FString::Printf(
			TEXT("%04d-%02d-%02d"),
			StartUtc.GetYear(),
			StartUtc.GetMonth(),
			StartUtc.GetDay());
}

static EBlueprintHelperMetricsLoadState BlueprintHelperMetricsResolveLoadState(
	const FBlueprintHelperMetricsLoadResult& LoadResult)
{
	if (!LoadResult.bSucceeded)
	{
		return EBlueprintHelperMetricsLoadState::Error;
	}
	if (LoadResult.Events.Num() == 0)
	{
		return EBlueprintHelperMetricsLoadState::Empty;
	}
	return EBlueprintHelperMetricsLoadState::Loaded;
}

static FString BlueprintHelperMetricsResolveStatusText(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot)
{
	if (Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Error)
	{
		return Snapshot.ErrorText.IsEmpty()
			? TEXT("Metrics load failed")
			: Snapshot.ErrorText;
	}
	if (Snapshot.Summary.TotalEvents == 0)
	{
		return TEXT("No Metrics data loaded");
	}
	return FString::Printf(
		TEXT("%d events across %d buckets"),
		Snapshot.Summary.TotalEvents,
		Snapshot.Buckets.Num());
}

static void BlueprintHelperMetricsTrimUsageRows(
	TArray<FBlueprintHelperMetricsUsageRow>& Rows,
	int32 Limit)
{
	Rows.Sort([](
		const FBlueprintHelperMetricsUsageRow& Left,
		const FBlueprintHelperMetricsUsageRow& Right)
	{
		if (Left.Total != Right.Total)
		{
			return Left.Total > Right.Total;
		}
		if (Left.Success != Right.Success)
		{
			return Left.Success > Right.Success;
		}
		if (Left.Failed != Right.Failed)
		{
			return Left.Failed > Right.Failed;
		}
		return Left.Name < Right.Name;
	});

	if (Rows.Num() > Limit)
	{
		Rows.SetNum(Limit);
	}
}

static void BlueprintHelperMetricsTrimTaskHealthRows(
	TArray<FBlueprintHelperMetricsTaskHealthRow>& Rows,
	int32 Limit)
{
	Rows.Sort([](
		const FBlueprintHelperMetricsTaskHealthRow& Left,
		const FBlueprintHelperMetricsTaskHealthRow& Right)
	{
		if (Left.FailedAttempts != Right.FailedAttempts)
		{
			return Left.FailedAttempts > Right.FailedAttempts;
		}

		const int32 LeftAttemptTotal = Left.PreviewAttempts + Left.ExecuteAttempts;
		const int32 RightAttemptTotal = Right.PreviewAttempts + Right.ExecuteAttempts;
		if (LeftAttemptTotal != RightAttemptTotal)
		{
			return LeftAttemptTotal > RightAttemptTotal;
		}
		if (Left.SuccessAttempts != Right.SuccessAttempts)
		{
			return Left.SuccessAttempts > Right.SuccessAttempts;
		}
		if (Left.TaskType != Right.TaskType)
		{
			return Left.TaskType < Right.TaskType;
		}
		if (Left.FeatureName != Right.FeatureName)
		{
			return Left.FeatureName < Right.FeatureName;
		}
		return Left.TargetType < Right.TargetType;
	});

	if (Rows.Num() > Limit)
	{
		Rows.SetNum(Limit);
	}
}

static void BlueprintHelperMetricsTrimErrorRows(
	TArray<FBlueprintHelperMetricsErrorRow>& Rows,
	int32 Limit)
{
	Rows.Sort([](
		const FBlueprintHelperMetricsErrorRow& Left,
		const FBlueprintHelperMetricsErrorRow& Right)
	{
		if (Left.Count != Right.Count)
		{
			return Left.Count > Right.Count;
		}
		if (Left.Category != Right.Category)
		{
			return Left.Category < Right.Category;
		}
		if (Left.ErrorCode != Right.ErrorCode)
		{
			return Left.ErrorCode < Right.ErrorCode;
		}
		return Left.IssueCode < Right.IssueCode;
	});

	if (Rows.Num() > Limit)
	{
		Rows.SetNum(Limit);
	}
}

static void BlueprintHelperMetricsTrimIoRows(
	TArray<FBlueprintHelperMetricsIoRow>& Rows,
	int32 Limit)
{
	Rows.Sort([](
		const FBlueprintHelperMetricsIoRow& Left,
		const FBlueprintHelperMetricsIoRow& Right)
	{
		const int64 LeftTokens = Left.EstimatedInputTokens + Left.EstimatedOutputTokens;
		const int64 RightTokens = Right.EstimatedInputTokens + Right.EstimatedOutputTokens;
		if (LeftTokens != RightTokens)
		{
			return LeftTokens > RightTokens;
		}
		if (Left.Total != Right.Total)
		{
			return Left.Total > Right.Total;
		}
		return Left.ToolName < Right.ToolName;
	});

	if (Rows.Num() > Limit)
	{
		Rows.SetNum(Limit);
	}
}

FBlueprintHelperMetricsPanelSnapshot FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(
	const FBlueprintHelperMetricsLoadResult& LoadResult,
	const FBlueprintHelperMetricsQuery& Query)
{
	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	Snapshot.TimelineMode = Query.TimelineMode;
	Snapshot.LoadState = BlueprintHelperMetricsResolveLoadState(LoadResult);
	Snapshot.MetricsRoot = LoadResult.MetricsRoot;
	Snapshot.ErrorText = LoadResult.Error;
	Snapshot.FilesRead = LoadResult.FilesRead;
	Snapshot.LinesRead = LoadResult.LinesRead;
	Snapshot.ParseWarnings = LoadResult.ParseWarnings;

	const FDateTime NowUtc = BlueprintHelperMetricsResolveNowUtc(Query);
	const int32 BucketCount = BlueprintHelperMetricsResolveBucketCount(Query);
	const int32 StepDays = BlueprintHelperMetricsResolveStepDays(Query.TimelineMode);
	const int32 TopRowLimit = BlueprintHelperMetricsResolveTopRowLimit(Query);
	const FDateTime CurrentBucketStartUtc =
		BlueprintHelperMetricsResolveCurrentBucketStartUtc(NowUtc, Query.TimelineMode);
	const FDateTime FirstBucketStartUtc =
		CurrentBucketStartUtc - FTimespan::FromDays((BucketCount - 1) * StepDays);
	const FDateTime LastBucketEndUtc =
		CurrentBucketStartUtc + FTimespan::FromDays(StepDays);

	Snapshot.Buckets.Reserve(BucketCount);
	for (int32 BucketIndex = 0; BucketIndex < BucketCount; ++BucketIndex)
	{
		FBlueprintHelperMetricsBucket Bucket;
		Bucket.StartUtc =
			FirstBucketStartUtc + FTimespan::FromDays(BucketIndex * StepDays);
		Bucket.EndUtc = Bucket.StartUtc + FTimespan::FromDays(StepDays);
		Bucket.BucketId = Bucket.StartUtc.ToIso8601();
		Bucket.Label = BlueprintHelperMetricsMakeBucketLabel(Bucket.StartUtc, Query.TimelineMode);
		Snapshot.Buckets.Add(MoveTemp(Bucket));
	}

	if (Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Error ||
		Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Empty)
	{
		Snapshot.StatusText = BlueprintHelperMetricsResolveStatusText(Snapshot);
		return Snapshot;
	}

	TMap<FString, FBlueprintHelperMetricsUsageAggregate> UsageAggregates;
	TMap<FString, FBlueprintHelperMetricsTaskHealthAggregate> TaskHealthAggregates;
	TMap<FString, FBlueprintHelperMetricsErrorAggregate> ErrorCategoryAggregates;
	TMap<FString, FBlueprintHelperMetricsErrorAggregate> TopErrorAggregates;
	TMap<FString, FBlueprintHelperMetricsIoAggregate> IoAggregates;

	for (const FBlueprintHelperMetricsEvent& Event : LoadResult.Events)
	{
		FDateTime TimestampUtc;
		if (!BlueprintHelperMetricsTryParseTimestamp(Event.Timestamp, TimestampUtc))
		{
			continue;
		}
		if (TimestampUtc < FirstBucketStartUtc || TimestampUtc >= LastBucketEndUtc)
		{
			continue;
		}

		const int32 BucketIndex = BlueprintHelperMetricsFindBucketIndex(
			TimestampUtc,
			FirstBucketStartUtc,
			BucketCount,
			StepDays);
		if (!Snapshot.Buckets.IsValidIndex(BucketIndex))
		{
			continue;
		}

		const bool bIsIoEvent = BlueprintHelperMetricsIsIoEvent(Event);
		const bool bIsSuccess = BlueprintHelperMetricsIsSuccessStatus(Event.Status);
		const bool bIsFailure = BlueprintHelperMetricsIsFailureStatus(Event.Status);
		const bool bIsTaskSpecPreview = BlueprintHelperMetricsIsTaskSpecPreviewEvent(Event);
		const bool bIsTaskSpecExecute = BlueprintHelperMetricsIsTaskSpecExecuteEvent(Event);

		FBlueprintHelperMetricsBucket& Bucket = Snapshot.Buckets[BucketIndex];
		++Snapshot.Summary.TotalEvents;
		++Bucket.TotalEvents;

		if (!bIsIoEvent)
		{
			++Bucket.ToolEvents;

			FBlueprintHelperMetricsUsageAggregate& UsageAggregate =
				UsageAggregates.FindOrAdd(BlueprintHelperMetricsNormalizeToolName(Event.ToolName));
			++UsageAggregate.Total;
			if (bIsSuccess)
			{
				++UsageAggregate.Success;
			}
			if (bIsFailure)
			{
				++UsageAggregate.Failed;
			}
		}

		if (bIsSuccess)
		{
			++Bucket.SuccessCount;
		}
		if (bIsFailure)
		{
			++Snapshot.Summary.TotalFailures;
			++Bucket.FailureCount;
			if (Event.ErrorCategory.IsEmpty())
			{
				++Snapshot.Summary.UnknownErrors;
			}

			const FString ErrorCategory = BlueprintHelperMetricsResolveErrorCategory(Event);
			Bucket.ErrorCategoryCounts.FindOrAdd(ErrorCategory) += 1;
			Bucket.TopErrorCounts.FindOrAdd(BlueprintHelperMetricsResolveTopErrorKey(Event)) += 1;

			FBlueprintHelperMetricsErrorAggregate& ErrorCategoryAggregate =
				ErrorCategoryAggregates.FindOrAdd(ErrorCategory);
			ErrorCategoryAggregate.Category = ErrorCategory;
			ErrorCategoryAggregate.Count += 1;

			FBlueprintHelperMetricsErrorAggregate& TopErrorAggregate =
				TopErrorAggregates.FindOrAdd(BlueprintHelperMetricsMakeTopErrorRowKey(Event));
			TopErrorAggregate.Category = ErrorCategory;
			TopErrorAggregate.ErrorCode = Event.ErrorCode;
			TopErrorAggregate.IssueCode = Event.bHasIssue ? Event.Issue.Code : FString();
			TopErrorAggregate.Count += 1;
		}

		if (bIsTaskSpecPreview)
		{
			++Bucket.TaskSpecPreviewAttempts;
		}
		if (bIsTaskSpecExecute)
		{
			++Bucket.TaskSpecExecuteAttempts;
		}

		if (Event.bHasTaskKey)
		{
			FBlueprintHelperMetricsTaskHealthAggregate& TaskAggregate =
				TaskHealthAggregates.FindOrAdd(BlueprintHelperMetricsMakeTaskHealthKey(Event));
			TaskAggregate.TaskType = Event.TaskKey.TaskType;
			TaskAggregate.FeatureName = Event.TaskKey.FeatureName;
			TaskAggregate.TargetType = Event.TaskKey.TargetType;
			if (bIsTaskSpecPreview)
			{
				++TaskAggregate.PreviewAttempts;
			}
			if (bIsTaskSpecExecute)
			{
				++TaskAggregate.ExecuteAttempts;
			}
			if (bIsSuccess)
			{
				++TaskAggregate.SuccessAttempts;
			}
			if (bIsFailure)
			{
				++TaskAggregate.FailedAttempts;
			}
		}

		if (bIsIoEvent && Event.bHasIo)
		{
			Bucket.InputChars += Event.Io.InputChars;
			Bucket.OutputChars += Event.Io.OutputChars;
			Bucket.InputUtf8Bytes += Event.Io.InputUtf8Bytes;
			Bucket.OutputUtf8Bytes += Event.Io.OutputUtf8Bytes;
			Bucket.EstimatedInputTokens += Event.Io.EstimatedInputTokens;
			Bucket.EstimatedOutputTokens += Event.Io.EstimatedOutputTokens;
			Snapshot.Summary.EstimatedInputTokens += Event.Io.EstimatedInputTokens;
			Snapshot.Summary.EstimatedOutputTokens += Event.Io.EstimatedOutputTokens;

			FBlueprintHelperMetricsIoAggregate& IoAggregate =
				IoAggregates.FindOrAdd(BlueprintHelperMetricsNormalizeToolName(Event.ToolName));
			IoAggregate.ToolName = BlueprintHelperMetricsNormalizeToolName(Event.ToolName);
			++IoAggregate.Total;
			IoAggregate.InputChars += Event.Io.InputChars;
			IoAggregate.OutputChars += Event.Io.OutputChars;
			IoAggregate.EstimatedInputTokens += Event.Io.EstimatedInputTokens;
			IoAggregate.EstimatedOutputTokens += Event.Io.EstimatedOutputTokens;
		}
	}

	Snapshot.ToolUsageRows.Reserve(UsageAggregates.Num());
	for (const TPair<FString, FBlueprintHelperMetricsUsageAggregate>& Pair : UsageAggregates)
	{
		FBlueprintHelperMetricsUsageRow Row;
		Row.Name = Pair.Key;
		Row.Total = Pair.Value.Total;
		Row.Success = Pair.Value.Success;
		Row.Failed = Pair.Value.Failed;
		Row.SuccessRate = Row.Total > 0
			? static_cast<float>(Row.Success) / static_cast<float>(Row.Total)
			: 0.0f;
		Snapshot.ToolUsageRows.Add(MoveTemp(Row));
	}
	BlueprintHelperMetricsTrimUsageRows(Snapshot.ToolUsageRows, TopRowLimit);

	Snapshot.TaskHealthRows.Reserve(TaskHealthAggregates.Num());
	for (const TPair<FString, FBlueprintHelperMetricsTaskHealthAggregate>& Pair : TaskHealthAggregates)
	{
		FBlueprintHelperMetricsTaskHealthRow Row;
		Row.TaskType = Pair.Value.TaskType;
		Row.FeatureName = Pair.Value.FeatureName;
		Row.TargetType = Pair.Value.TargetType;
		Row.PreviewAttempts = Pair.Value.PreviewAttempts;
		Row.ExecuteAttempts = Pair.Value.ExecuteAttempts;
		Row.FailedAttempts = Pair.Value.FailedAttempts;
		Row.SuccessAttempts = Pair.Value.SuccessAttempts;
		Snapshot.TaskHealthRows.Add(MoveTemp(Row));
	}
	BlueprintHelperMetricsTrimTaskHealthRows(Snapshot.TaskHealthRows, TopRowLimit);

	Snapshot.ErrorCategoryRows.Reserve(ErrorCategoryAggregates.Num());
	for (const TPair<FString, FBlueprintHelperMetricsErrorAggregate>& Pair : ErrorCategoryAggregates)
	{
		FBlueprintHelperMetricsErrorRow Row;
		Row.Category = Pair.Value.Category;
		Row.Count = Pair.Value.Count;
		Snapshot.ErrorCategoryRows.Add(MoveTemp(Row));
	}
	BlueprintHelperMetricsTrimErrorRows(Snapshot.ErrorCategoryRows, TopRowLimit);

	Snapshot.TopErrorRows.Reserve(TopErrorAggregates.Num());
	for (const TPair<FString, FBlueprintHelperMetricsErrorAggregate>& Pair : TopErrorAggregates)
	{
		FBlueprintHelperMetricsErrorRow Row;
		Row.Category = Pair.Value.Category;
		Row.ErrorCode = Pair.Value.ErrorCode;
		Row.IssueCode = Pair.Value.IssueCode;
		Row.Count = Pair.Value.Count;
		Snapshot.TopErrorRows.Add(MoveTemp(Row));
	}
	BlueprintHelperMetricsTrimErrorRows(Snapshot.TopErrorRows, TopRowLimit);

	Snapshot.IoUsageRows.Reserve(IoAggregates.Num());
	for (const TPair<FString, FBlueprintHelperMetricsIoAggregate>& Pair : IoAggregates)
	{
		FBlueprintHelperMetricsIoRow Row;
		Row.ToolName = Pair.Value.ToolName;
		Row.Total = Pair.Value.Total;
		Row.InputChars = Pair.Value.InputChars;
		Row.OutputChars = Pair.Value.OutputChars;
		Row.EstimatedInputTokens = Pair.Value.EstimatedInputTokens;
		Row.EstimatedOutputTokens = Pair.Value.EstimatedOutputTokens;
		Snapshot.IoUsageRows.Add(MoveTemp(Row));
	}
	BlueprintHelperMetricsTrimIoRows(Snapshot.IoUsageRows, TopRowLimit);

	if (Snapshot.Summary.TotalEvents == 0)
	{
		Snapshot.LoadState = EBlueprintHelperMetricsLoadState::Empty;
	}
	Snapshot.StatusText = BlueprintHelperMetricsResolveStatusText(Snapshot);
	return Snapshot;
}
