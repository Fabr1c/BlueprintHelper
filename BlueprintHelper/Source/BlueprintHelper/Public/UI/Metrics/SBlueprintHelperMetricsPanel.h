// BlueprintHelper Metrics Slate panel.

#pragma once

#include "CoreMinimal.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelData.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperMetricsPanelPresenter;
class SBorder;
class SBlueprintHelperMetricsDetailChart;
class SBlueprintHelperMetricsMetricSelector;
class SBlueprintHelperMetricsOverviewChart;
class STextBlock;
class SWidget;

class BLUEPRINTHELPER_API SBlueprintHelperMetricsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	static void ShutdownAsyncTasks();

private:
	void HandlePresenterEvent(const FBlueprintHelperMetricsPanelPresenterEvent& Event);
	void RefreshFromSnapshot(
		const FBlueprintHelperMetricsPanelSnapshot& Snapshot,
		EBlueprintHelperMetricsPanelUpdateScope UpdateScope);

	TSharedRef<SWidget> BuildContent();
	TSharedRef<SWidget> BuildStatusContent();
	TSharedRef<SWidget> BuildLoadedContent();
	TSharedRef<SWidget> BuildAbcLayout();
	void ApplySnapshotToRegions(
		const FBlueprintHelperMetricsPanelSnapshot& Snapshot,
		EBlueprintHelperMetricsPanelUpdateScope UpdateScope);
	void UpdateStatusText(const FBlueprintHelperMetricsPanelSnapshot& Snapshot);
	FString BuildOverviewTitle() const;
	FString BuildOverviewSubtitle() const;
	FString BuildDetailTotalText() const;
	FString GetSelectedMetricUnitLabel() const;
	void HandleMetricSelected(EBlueprintHelperMetricsMetricKind MetricKind);
	void HandleTimelineModeSelected(EBlueprintHelperMetricsTimelineMode TimelineMode);
	void HandleBucketSelected(const FString& BucketId);
	void HandleRefreshRequested();

	TSharedPtr<FBlueprintHelperMetricsPanelPresenter> Presenter;
	FBlueprintHelperMetricsPanelSnapshot CurrentSnapshot;
	TSharedPtr<SBorder> ContentHost;
	TSharedPtr<SBlueprintHelperMetricsMetricSelector> MetricSelectorWidget;
	TSharedPtr<SBlueprintHelperMetricsOverviewChart> OverviewChartWidget;
	TSharedPtr<SBlueprintHelperMetricsDetailChart> DetailChartWidget;
	TSharedPtr<STextBlock> StatusTextWidget;
};
