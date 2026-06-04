// BlueprintHelper Metrics overview chart widget.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(
	FOnBlueprintHelperMetricsTimelineModeSelected,
	EBlueprintHelperMetricsTimelineMode);
DECLARE_DELEGATE_OneParam(
	FOnBlueprintHelperMetricsBucketSelected,
	const FString&);
DECLARE_DELEGATE(FOnBlueprintHelperMetricsRefreshClicked);

class SBlueprintHelperMetricsOverviewChart : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsOverviewChart)
	{
	}
		SLATE_ARGUMENT(EBlueprintHelperMetricsTimelineMode, TimelineMode)
		SLATE_ARGUMENT(TArray<FBlueprintHelperMetricsOverviewBarView>, Bars)
		SLATE_EVENT(FOnBlueprintHelperMetricsTimelineModeSelected, OnTimelineModeSelected)
		SLATE_EVENT(FOnBlueprintHelperMetricsBucketSelected, OnBucketSelected)
		SLATE_EVENT(FOnBlueprintHelperMetricsRefreshClicked, OnRefreshClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildBar(
		const FBlueprintHelperMetricsOverviewBarView& Bar) const;
	FReply HandleDailyClicked() const;
	FReply HandleWeeklyClicked() const;
	FReply HandleRefreshClicked() const;
	FReply HandleBarClicked(FString BucketId) const;

	EBlueprintHelperMetricsTimelineMode TimelineMode =
		EBlueprintHelperMetricsTimelineMode::Daily;
	TArray<FBlueprintHelperMetricsOverviewBarView> Bars;
	FOnBlueprintHelperMetricsTimelineModeSelected OnTimelineModeSelected;
	FOnBlueprintHelperMetricsBucketSelected OnBucketSelected;
	FOnBlueprintHelperMetricsRefreshClicked OnRefreshClicked;
};
