// BlueprintHelper Metrics panel presenter.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelData.h"

class BLUEPRINTHELPER_API FBlueprintHelperMetricsPanelPresenter
	: public TSharedFromThis<FBlueprintHelperMetricsPanelPresenter>
{
public:
	using FPresenterEventSink = TFunction<void(const FBlueprintHelperMetricsPanelPresenterEvent&)>;
	using FLoadSnapshot = TFunction<FBlueprintHelperMetricsPanelSnapshot(
		EBlueprintHelperMetricsTimelineMode)>;

	static TSharedRef<FBlueprintHelperMetricsPanelPresenter> CreateDefault();

	FBlueprintHelperMetricsPanelPresenter();
	explicit FBlueprintHelperMetricsPanelPresenter(FLoadSnapshot InLoadSnapshot);

	void SetEventSink(FPresenterEventSink InEventSink);
	FReply HandleVisualEvent(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	FReply HandleVisualEventForTests(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	const FBlueprintHelperMetricsPanelSnapshot& GetSnapshot() const;

private:
	FReply HandleRefreshRequest(
		EBlueprintHelperMetricsTimelineMode TimelineMode,
		bool bUseAsync);
	void ApplySnapshotAndEmit(const FBlueprintHelperMetricsPanelSnapshot& InSnapshot);
	void EmitCurrentSnapshot() const;
	void QueuePendingRefreshRequest(EBlueprintHelperMetricsTimelineMode TimelineMode);
	bool ConsumePendingRefreshRequest(EBlueprintHelperMetricsTimelineMode& OutTimelineMode);
	void CompleteRefreshRequest(
		const FBlueprintHelperMetricsPanelSnapshot& LoadedSnapshot,
		bool bUseAsync);
	FBlueprintHelperMetricsPanelSnapshot BuildLoadingSnapshot(
		EBlueprintHelperMetricsTimelineMode TimelineMode) const;
	FBlueprintHelperMetricsPanelSnapshot BuildErrorSnapshot(
		EBlueprintHelperMetricsTimelineMode TimelineMode,
		const FString& ErrorText) const;
	FBlueprintHelperMetricsPanelSnapshot LoadSnapshot(
		EBlueprintHelperMetricsTimelineMode TimelineMode) const;

	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	FPresenterEventSink EventSink;
	FLoadSnapshot LoadSnapshotCallback;
	bool bLoadInProgress = false;
	bool bPendingLoadRequest = false;
	EBlueprintHelperMetricsTimelineMode PendingTimelineMode =
		EBlueprintHelperMetricsTimelineMode::Daily;
};
