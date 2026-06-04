// BlueprintHelper Metrics panel UI event models.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"

enum class EBlueprintHelperMetricsVisualEventType : uint8
{
	RefreshClicked,
	TimelineModeChanged
};

struct BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelVisualEvent
{
	EBlueprintHelperMetricsVisualEventType Type =
		EBlueprintHelperMetricsVisualEventType::RefreshClicked;
	EBlueprintHelperMetricsTimelineMode TimelineMode = EBlueprintHelperMetricsTimelineMode::Daily;

	static FBlueprintHelperMetricsPanelVisualEvent RefreshClicked();
	static FBlueprintHelperMetricsPanelVisualEvent TimelineModeChanged(
		EBlueprintHelperMetricsTimelineMode InTimelineMode);
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
FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
	EBlueprintHelperMetricsTimelineMode InTimelineMode)
{
	FBlueprintHelperMetricsPanelVisualEvent Event;
	Event.Type = EBlueprintHelperMetricsVisualEventType::TimelineModeChanged;
	Event.TimelineMode = InTimelineMode;
	return Event;
}
