// BlueprintHelper Metrics data models.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperMetricsTimelineMode : uint8
{
	Daily,
	Weekly
};

enum class EBlueprintHelperMetricsLoadState : uint8
{
	Empty,
	Loading,
	Loaded,
	Error
};

enum class EBlueprintHelperMetricsMetricKind : uint8
{
	ToolUsage,
	TaskSpecAttempts,
	ErrorCategories,
	TopErrors,
	OperationUsage,
	CliIoUsage,
	EstimatedTokens
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsTaskKey
{
	FString TaskType;
	FString FeatureName;
	FString TargetType;
	FString TargetRefHash;
	FString TargetRefLabel;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsIssueSummary
{
	FString Code;
	FString Path;
	FString MessageDigest;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsIoSummary
{
	FString InputSource;
	int64 InputChars = 0;
	int64 InputUtf8Bytes = 0;
	int64 OutputChars = 0;
	int64 OutputUtf8Bytes = 0;
	int64 EstimatedInputTokens = 0;
	int64 EstimatedOutputTokens = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsEvent
{
	FString Timestamp;
	FString EventType;
	FString ToolName;
	FString Status;
	FString ErrorCategory;
	FString ErrorCode;
	FString Capability;
	FString SemanticOperation;
	FBlueprintHelperMetricsTaskKey TaskKey;
	FBlueprintHelperMetricsIssueSummary Issue;
	FBlueprintHelperMetricsIoSummary Io;
	int64 DurationMs = 0;
	bool bHasTaskKey = false;
	bool bHasIssue = false;
	bool bHasIo = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsLoadResult
{
	TArray<FBlueprintHelperMetricsEvent> Events;
	FString MetricsRoot;
	int32 FilesRead = 0;
	int32 LinesRead = 0;
	int32 LinesSkipped = 0;
	int32 ParseWarnings = 0;
	FString Error;
	bool bSucceeded = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsQuery
{
	EBlueprintHelperMetricsTimelineMode TimelineMode = EBlueprintHelperMetricsTimelineMode::Daily;
	EBlueprintHelperMetricsMetricKind MetricKind =
		EBlueprintHelperMetricsMetricKind::ToolUsage;
	FString SelectedBucketId;
	FDateTime NowUtc = FDateTime();
	int32 DailyBucketCount = 14;
	int32 WeeklyBucketCount = 8;
	int32 TopRowLimit = 10;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsBucket
{
	FString BucketId;
	FString Label;
	FDateTime StartUtc;
	FDateTime EndUtc;
	int32 TotalEvents = 0;
	int32 ToolEvents = 0;
	int32 OperationEvents = 0;
	int32 TaskSpecPreviewAttempts = 0;
	int32 TaskSpecExecuteAttempts = 0;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	TMap<FString, int32> ErrorCategoryCounts;
	TMap<FString, int32> TopErrorCounts;
	int64 InputChars = 0;
	int64 OutputChars = 0;
	int64 InputUtf8Bytes = 0;
	int64 OutputUtf8Bytes = 0;
	int64 EstimatedInputTokens = 0;
	int64 EstimatedOutputTokens = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsSummary
{
	int32 TotalEvents = 0;
	int32 TotalFailures = 0;
	int32 UnknownErrors = 0;
	int64 EstimatedInputTokens = 0;
	int64 EstimatedOutputTokens = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsUsageRow
{
	FString Name;
	int32 Total = 0;
	int32 Success = 0;
	int32 Failed = 0;
	float SuccessRate = 0.0f;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsTaskHealthRow
{
	FString TaskType;
	FString FeatureName;
	FString TargetType;
	int32 PreviewAttempts = 0;
	int32 ExecuteAttempts = 0;
	int32 FailedAttempts = 0;
	int32 SuccessAttempts = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsErrorRow
{
	FString Category;
	FString ErrorCode;
	FString IssueCode;
	int32 Count = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsIoRow
{
	FString ToolName;
	int32 Total = 0;
	int64 InputChars = 0;
	int64 OutputChars = 0;
	int64 EstimatedInputTokens = 0;
	int64 EstimatedOutputTokens = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelSelection
{
	EBlueprintHelperMetricsMetricKind MetricKind =
		EBlueprintHelperMetricsMetricKind::ToolUsage;
	EBlueprintHelperMetricsTimelineMode TimelineMode =
		EBlueprintHelperMetricsTimelineMode::Daily;
	FString SelectedBucketId;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsMetricOptionView
{
	EBlueprintHelperMetricsMetricKind Kind =
		EBlueprintHelperMetricsMetricKind::ToolUsage;
	FString Label;
	FString Description;
	FString UnitLabel;
	int64 Total = 0;
	bool bIsSelected = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsOverviewBarView
{
	FString BucketId;
	FString Label;
	FDateTime StartUtc;
	FDateTime EndUtc;
	int64 Value = 0;
	int64 SecondaryValue = 0;
	float Fraction = 0.0f;
	FString ValueLabel;
	FString Detail;
	bool bIsSelected = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsDetailBarView
{
	FString Label;
	FString SubText;
	FString UnitLabel;
	int64 Value = 0;
	int64 SecondaryValue = 0;
	float Fraction = 0.0f;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsOperationUsageRow
{
	FString Capability;
	FString SemanticOperation;
	int32 Total = 0;
	int32 Success = 0;
	int32 Failed = 0;
	float SuccessRate = 0.0f;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelSnapshot
{
	EBlueprintHelperMetricsTimelineMode TimelineMode = EBlueprintHelperMetricsTimelineMode::Daily;
	EBlueprintHelperMetricsLoadState LoadState = EBlueprintHelperMetricsLoadState::Empty;
	FBlueprintHelperMetricsPanelSelection Selection;
	FBlueprintHelperMetricsSummary Summary;
	TArray<FBlueprintHelperMetricsBucket> Buckets;
	TArray<FBlueprintHelperMetricsMetricOptionView> MetricOptions;
	TArray<FBlueprintHelperMetricsOverviewBarView> OverviewBars;
	TArray<FBlueprintHelperMetricsDetailBarView> DetailBars;
	TArray<FBlueprintHelperMetricsOperationUsageRow> OperationUsageRows;
	TArray<FBlueprintHelperMetricsUsageRow> ToolUsageRows;
	TArray<FBlueprintHelperMetricsTaskHealthRow> TaskHealthRows;
	TArray<FBlueprintHelperMetricsErrorRow> ErrorCategoryRows;
	TArray<FBlueprintHelperMetricsErrorRow> TopErrorRows;
	TArray<FBlueprintHelperMetricsIoRow> IoUsageRows;
	FString SelectedMetricTitle;
	FString SelectedBucketLabel;
	int64 SelectedBucketTotal = 0;
	FString MetricsRoot;
	FString StatusText = TEXT("No Metrics data loaded");
	FString ErrorText;
	int32 FilesRead = 0;
	int32 LinesRead = 0;
	int32 ParseWarnings = 0;
};
