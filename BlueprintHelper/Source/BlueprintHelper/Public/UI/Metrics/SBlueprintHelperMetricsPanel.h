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
	FReply OnRefreshClicked();
	FReply OnDailyClicked();
	FReply OnWeeklyClicked();
	void HandlePresenterEvent(const FBlueprintHelperMetricsPanelPresenterEvent& Event);
	void RefreshFromSnapshot(const FBlueprintHelperMetricsPanelSnapshot& Snapshot);

	TSharedRef<SWidget> BuildContent();
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusContent();
	TSharedRef<SWidget> BuildLoadedContent();
	TSharedRef<SWidget> BuildSummary();
	TSharedRef<SWidget> BuildBucketBars();
	TSharedRef<SWidget> BuildUsageRows(
		const FString& Title,
		const TArray<FBlueprintHelperMetricsUsageRow>& Rows);
	TSharedRef<SWidget> BuildTaskHealthRows();
	TSharedRef<SWidget> BuildErrorRows(
		const FString& Title,
		const TArray<FBlueprintHelperMetricsErrorRow>& Rows);
	TSharedRef<SWidget> BuildIoRows();
	TSharedRef<SWidget> BuildEmptySection(const FString& Title);
	TSharedRef<SWidget> BuildBarRow(
		const FString& Label,
		int64 Value,
		int64 MaxValue,
		const FString& Detail);
	FSlateColor GetDailyButtonColor() const;
	FSlateColor GetWeeklyButtonColor() const;

	TSharedPtr<FBlueprintHelperMetricsPanelPresenter> Presenter;
	FBlueprintHelperMetricsPanelSnapshot CurrentSnapshot;
	TSharedPtr<SBorder> ContentHost;
};
