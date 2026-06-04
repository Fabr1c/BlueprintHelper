// BlueprintHelper Metrics Slate panel.

#pragma once

#include "CoreMinimal.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelData.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperMetricsPanelPresenter;
class SBorder;
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
	void RefreshFromSnapshot(const FBlueprintHelperMetricsPanelSnapshot& Snapshot);

	TSharedRef<SWidget> BuildContent();
	TSharedRef<SWidget> BuildStatusContent();
	TSharedRef<SWidget> BuildLoadedContent();
	TSharedRef<SWidget> BuildAbcLayout();
	FString GetSelectedMetricUnitLabel() const;
	void HandleMetricSelected(EBlueprintHelperMetricsMetricKind MetricKind);
	void HandleTimelineModeSelected(EBlueprintHelperMetricsTimelineMode TimelineMode);
	void HandleBucketSelected(const FString& BucketId);
	void HandleRefreshRequested();

	TSharedPtr<FBlueprintHelperMetricsPanelPresenter> Presenter;
	FBlueprintHelperMetricsPanelSnapshot CurrentSnapshot;
	TSharedPtr<SBorder> ContentHost;
};
