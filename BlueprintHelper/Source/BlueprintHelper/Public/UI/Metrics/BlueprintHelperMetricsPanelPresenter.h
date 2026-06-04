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
		const FBlueprintHelperMetricsPanelSelection&)>;
	using FLoadMetricsResult = TFunction<FBlueprintHelperMetricsLoadResult()>;

	static TSharedRef<FBlueprintHelperMetricsPanelPresenter> CreateDefault();

	FBlueprintHelperMetricsPanelPresenter();
	explicit FBlueprintHelperMetricsPanelPresenter(FLoadSnapshot InLoadSnapshot);
	explicit FBlueprintHelperMetricsPanelPresenter(FLoadMetricsResult InLoadMetricsResult);

	void SetEventSink(FPresenterEventSink InEventSink);
	FReply HandleVisualEvent(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	FReply HandleVisualEventForTests(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	const FBlueprintHelperMetricsPanelSnapshot& GetSnapshot() const;

private:
	struct FLoadCompletion
	{
		FBlueprintHelperMetricsPanelSnapshot Snapshot;
		FBlueprintHelperMetricsLoadResult LoadResult;
		bool bHasLoadResult = false;
	};

	FReply HandleRefreshRequest(
		const FBlueprintHelperMetricsPanelSelection& Selection,
		bool bUseAsync);
	FReply HandleCachedSelectionRequest(
		const FBlueprintHelperMetricsPanelSelection& Selection,
		EBlueprintHelperMetricsPanelUpdateScope UpdateScope,
		bool bUseAsync);
	void ApplySnapshotAndEmit(
		const FBlueprintHelperMetricsPanelSnapshot& InSnapshot,
		EBlueprintHelperMetricsPanelUpdateScope UpdateScope);
	void EmitCurrentSnapshot(EBlueprintHelperMetricsPanelUpdateScope UpdateScope) const;
	void QueuePendingRefreshRequest(
		const FBlueprintHelperMetricsPanelSelection& Selection);
	bool ConsumePendingRefreshRequest(
		FBlueprintHelperMetricsPanelSelection& OutSelection);
	void CompleteRefreshRequest(
		const FLoadCompletion& Completion,
		bool bUseAsync);
	void CacheLoadCompletion(const FLoadCompletion& Completion);
	FBlueprintHelperMetricsPanelSnapshot BuildLoadingSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FBlueprintHelperMetricsPanelSnapshot BuildRefreshingSnapshotFromCurrent(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FBlueprintHelperMetricsPanelSnapshot BuildErrorSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection,
		const FString& ErrorText) const;
	FBlueprintHelperMetricsPanelSnapshot BuildSnapshotFromCachedProjection(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FBlueprintHelperMetricsPanelSnapshot BuildSnapshotFromLoadResult(
		const FBlueprintHelperMetricsLoadResult& LoadResult,
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FBlueprintHelperMetricsQuery BuildQuery(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FLoadCompletion LoadSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	bool CanProjectFromCache() const;

	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	FPresenterEventSink EventSink;
	FLoadSnapshot LoadSnapshotCallback;
	FLoadMetricsResult LoadMetricsResultCallback;
	FBlueprintHelperMetricsLoadResult CachedLoadResult;
	bool bCachedLoadResultAvailable = false;
	bool bLoadInProgress = false;
	bool bPendingLoadRequest = false;
	FBlueprintHelperMetricsPanelSelection PendingSelection;
};
