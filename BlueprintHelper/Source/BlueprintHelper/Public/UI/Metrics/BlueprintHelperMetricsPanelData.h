// BlueprintHelper Metrics panel UI event models.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"

enum class EBlueprintHelperMetricsVisualEventType : uint8
{
	RefreshClicked,
	MetricSelected,
	TimelineModeChanged,
	OverviewBucketSelected
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelVisualEvent
{
	EBlueprintHelperMetricsVisualEventType Type =
		EBlueprintHelperMetricsVisualEventType::RefreshClicked;
	EBlueprintHelperMetricsMetricKind MetricKind =
		EBlueprintHelperMetricsMetricKind::ToolUsage;
	EBlueprintHelperMetricsTimelineMode TimelineMode = EBlueprintHelperMetricsTimelineMode::Daily;
	FString BucketId;

	static FBlueprintHelperMetricsPanelVisualEvent RefreshClicked();
	static FBlueprintHelperMetricsPanelVisualEvent MetricSelected(
		EBlueprintHelperMetricsMetricKind InMetricKind);
	static FBlueprintHelperMetricsPanelVisualEvent TimelineModeChanged(
		EBlueprintHelperMetricsTimelineMode InTimelineMode);
	static FBlueprintHelperMetricsPanelVisualEvent OverviewBucketSelected(
		const FString& InBucketId);
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelPresenterEvent
{
	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	bool bRefreshView = false;
};

inline FBlueprintHelperMetricsPanelVisualEvent
FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked()
{
	FBlueprintHelperMetricsPanelVisualEvent Event;
	Event.Type = EBlueprintHelperMetricsVisualEventType::RefreshClicked;
	return Event;
}

inline FBlueprintHelperMetricsPanelVisualEvent
FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(
	EBlueprintHelperMetricsMetricKind InMetricKind)
{
	FBlueprintHelperMetricsPanelVisualEvent Event;
	Event.Type = EBlueprintHelperMetricsVisualEventType::MetricSelected;
	Event.MetricKind = InMetricKind;
	return Event;
}

inline FBlueprintHelperMetricsPanelVisualEvent
FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
	EBlueprintHelperMetricsTimelineMode InTimelineMode)
{
	FBlueprintHelperMetricsPanelVisualEvent Event;
	Event.Type = EBlueprintHelperMetricsVisualEventType::TimelineModeChanged;
	Event.TimelineMode = InTimelineMode;
	return Event;
}

inline FBlueprintHelperMetricsPanelVisualEvent
FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(
	const FString& InBucketId)
{
	FBlueprintHelperMetricsPanelVisualEvent Event;
	Event.Type = EBlueprintHelperMetricsVisualEventType::OverviewBucketSelected;
	Event.BucketId = InBucketId;
	return Event;
}
