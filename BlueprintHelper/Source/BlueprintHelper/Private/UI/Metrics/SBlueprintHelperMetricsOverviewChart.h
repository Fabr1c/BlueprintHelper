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

class SVerticalBox;

class SBlueprintHelperMetricsOverviewChart : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsOverviewChart)
	{
	}
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(FString, Subtitle)
		SLATE_ARGUMENT(EBlueprintHelperMetricsTimelineMode, TimelineMode)
		SLATE_ARGUMENT(FBlueprintHelperMetricsSummary, Summary)
		SLATE_ARGUMENT(TArray<FBlueprintHelperMetricsOverviewBarView>, Bars)
		SLATE_ARGUMENT(bool, bRefreshInProgress)
		SLATE_EVENT(FOnBlueprintHelperMetricsTimelineModeSelected, OnTimelineModeSelected)
		SLATE_EVENT(FOnBlueprintHelperMetricsBucketSelected, OnBucketSelected)
		SLATE_EVENT(FOnBlueprintHelperMetricsRefreshClicked, OnRefreshClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetData(
		const FString& InTitle,
		const FString& InSubtitle,
		EBlueprintHelperMetricsTimelineMode InTimelineMode,
		const FBlueprintHelperMetricsSummary& InSummary,
		const TArray<FBlueprintHelperMetricsOverviewBarView>& InBars,
		bool bInRefreshInProgress);

private:
	void RefreshView();
	TSharedRef<SWidget> BuildSummaryCard(
		const FString& Label,
		const FString& ValueText,
		const FString& CaptionText) const;
	TSharedRef<SWidget> BuildBar(
		const FBlueprintHelperMetricsOverviewBarView& Bar) const;
	FReply HandleDailyClicked() const;
	FReply HandleWeeklyClicked() const;
	FReply HandleRefreshClicked() const;
	FReply HandleBarClicked(FString BucketId) const;

	FString Title;
	FString Subtitle;
	EBlueprintHelperMetricsTimelineMode TimelineMode =
		EBlueprintHelperMetricsTimelineMode::Daily;
	FBlueprintHelperMetricsSummary Summary;
	TArray<FBlueprintHelperMetricsOverviewBarView> Bars;
	bool bRefreshInProgress = false;
	FOnBlueprintHelperMetricsTimelineModeSelected OnTimelineModeSelected;
	FOnBlueprintHelperMetricsBucketSelected OnBucketSelected;
	FOnBlueprintHelperMetricsRefreshClicked OnRefreshClicked;
	TSharedPtr<SVerticalBox> RootBox;
};
