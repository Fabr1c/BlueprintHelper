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

	static TSharedRef<FBlueprintHelperMetricsPanelPresenter> CreateDefault();

	FBlueprintHelperMetricsPanelPresenter();
	explicit FBlueprintHelperMetricsPanelPresenter(FLoadSnapshot InLoadSnapshot);

	void SetEventSink(FPresenterEventSink InEventSink);
	FReply HandleVisualEvent(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	FReply HandleVisualEventForTests(const FBlueprintHelperMetricsPanelVisualEvent& Event);
	const FBlueprintHelperMetricsPanelSnapshot& GetSnapshot() const;

private:
	FReply HandleRefreshRequest(
		const FBlueprintHelperMetricsPanelSelection& Selection,
		bool bUseAsync);
	void ApplySnapshotAndEmit(const FBlueprintHelperMetricsPanelSnapshot& InSnapshot);
	void EmitCurrentSnapshot() const;
	void QueuePendingRefreshRequest(
		const FBlueprintHelperMetricsPanelSelection& Selection);
	bool ConsumePendingRefreshRequest(
		FBlueprintHelperMetricsPanelSelection& OutSelection);
	void CompleteRefreshRequest(
		const FBlueprintHelperMetricsPanelSnapshot& LoadedSnapshot,
		bool bUseAsync);
	FBlueprintHelperMetricsPanelSnapshot BuildLoadingSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;
	FBlueprintHelperMetricsPanelSnapshot BuildErrorSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection,
		const FString& ErrorText) const;
	FBlueprintHelperMetricsPanelSnapshot LoadSnapshot(
		const FBlueprintHelperMetricsPanelSelection& Selection) const;

	FBlueprintHelperMetricsPanelSnapshot Snapshot;
	FPresenterEventSink EventSink;
	FLoadSnapshot LoadSnapshotCallback;
	bool bLoadInProgress = false;
	bool bPendingLoadRequest = false;
	FBlueprintHelperMetricsPanelSelection PendingSelection;
};
