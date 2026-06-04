// BlueprintHelper Metrics panel presenter implementation.

#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"

#include "Async/Async.h"
#include "Systems/Metrics/BlueprintHelperMetricsStoreReader.h"
#include "Systems/Metrics/BlueprintHelperMetricsTimeSeriesService.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"

static FBlueprintHelperMetricsPanelSnapshot
BlueprintHelperMetricsPanelLoadDefaultSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	FBlueprintHelperMetricsQuery Query;
	Query.TimelineMode = Selection.TimelineMode;
	Query.MetricKind = Selection.MetricKind;
	Query.SelectedBucketId = Selection.SelectedBucketId;
	Query.NowUtc = FDateTime::UtcNow();

	const FBlueprintHelperMetricsLoadResult LoadResult =
		FBlueprintHelperMetricsStoreReader::LoadDefault();
	return FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(LoadResult, Query);
}

static FString BlueprintHelperMetricsPanelLoadingStatusText()
{
	return TEXT("Loading Metrics data...");
}

static FString BlueprintHelperMetricsPanelMissingLoaderText()
{
	return TEXT("Metrics load callback is not configured");
}

static FString BlueprintHelperMetricsPanelShutdownText()
{
	return TEXT("Metrics refresh skipped: worker is shutting down");
}

static FBlueprintHelperMetricsPanelSelection
BlueprintHelperMetricsPanelSelectionFromEvent(
	const FBlueprintHelperMetricsPanelSelection& CurrentSelection,
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	FBlueprintHelperMetricsPanelSelection NextSelection = CurrentSelection;
	switch (Event.Type)
	{
	case EBlueprintHelperMetricsVisualEventType::RefreshClicked:
		break;
	case EBlueprintHelperMetricsVisualEventType::MetricSelected:
		NextSelection.MetricKind = Event.MetricKind;
		NextSelection.SelectedBucketId.Reset();
		break;
	case EBlueprintHelperMetricsVisualEventType::TimelineModeChanged:
		NextSelection.TimelineMode = Event.TimelineMode;
		NextSelection.SelectedBucketId.Reset();
		break;
	case EBlueprintHelperMetricsVisualEventType::OverviewBucketSelected:
		NextSelection.SelectedBucketId = Event.BucketId;
		break;
	default:
		break;
	}
	return NextSelection;
}

TSharedRef<FBlueprintHelperMetricsPanelPresenter>
FBlueprintHelperMetricsPanelPresenter::CreateDefault()
{
	return MakeShared<FBlueprintHelperMetricsPanelPresenter>();
}

FBlueprintHelperMetricsPanelPresenter::FBlueprintHelperMetricsPanelPresenter()
	: LoadSnapshotCallback(&BlueprintHelperMetricsPanelLoadDefaultSnapshot)
{
}

FBlueprintHelperMetricsPanelPresenter::FBlueprintHelperMetricsPanelPresenter(
	FLoadSnapshot InLoadSnapshot)
	: LoadSnapshotCallback(MoveTemp(InLoadSnapshot))
{
}

void FBlueprintHelperMetricsPanelPresenter::SetEventSink(
	FPresenterEventSink InEventSink)
{
	EventSink = MoveTemp(InEventSink);
	EmitCurrentSnapshot();
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleVisualEvent(
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	const FBlueprintHelperMetricsPanelSelection NextSelection =
		BlueprintHelperMetricsPanelSelectionFromEvent(Snapshot.Selection, Event);
	return HandleRefreshRequest(NextSelection, true);
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleVisualEventForTests(
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	const FBlueprintHelperMetricsPanelSelection NextSelection =
		BlueprintHelperMetricsPanelSelectionFromEvent(Snapshot.Selection, Event);
	return HandleRefreshRequest(NextSelection, false);
}

const FBlueprintHelperMetricsPanelSnapshot&
FBlueprintHelperMetricsPanelPresenter::GetSnapshot() const
{
	return Snapshot;
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleRefreshRequest(
	const FBlueprintHelperMetricsPanelSelection& Selection,
	bool bUseAsync)
{
	if (bLoadInProgress)
	{
		QueuePendingRefreshRequest(Selection);
		return FReply::Handled();
	}

	if (bUseAsync &&
		FBlueprintHelperMetricsPanelAsyncUtils::IsShutdownRequested())
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelShutdownText()));
		return FReply::Handled();
	}

	if (!LoadSnapshotCallback)
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelMissingLoaderText()));
		return FReply::Handled();
	}

	bLoadInProgress = true;
	ApplySnapshotAndEmit(BuildLoadingSnapshot(Selection));

	if (!bUseAsync)
	{
		const FBlueprintHelperMetricsPanelSnapshot LoadedSnapshot =
			LoadSnapshot(Selection);
		CompleteRefreshRequest(LoadedSnapshot, false);
		return FReply::Handled();
	}

	const FBlueprintHelperMetricsPanelSelection RequestedSelection = Selection;
	TWeakPtr<FBlueprintHelperMetricsPanelPresenter> WeakPresenter = AsShared();
	const FLoadSnapshot Loader = LoadSnapshotCallback;
	TFuture<void> LoadTask = Async(
		EAsyncExecution::ThreadPool,
		[WeakPresenter, Loader, RequestedSelection]()
		{
			FBlueprintHelperMetricsPanelSnapshot LoadedSnapshot;
			if (Loader)
			{
				LoadedSnapshot = Loader(RequestedSelection);
			}

			AsyncTask(
				ENamedThreads::GameThread,
				[WeakPresenter, LoadedSnapshot]() mutable
				{
					const TSharedPtr<FBlueprintHelperMetricsPanelPresenter> Presenter =
						WeakPresenter.Pin();
					if (!Presenter.IsValid())
					{
						return;
					}

					if (FBlueprintHelperMetricsPanelAsyncUtils::IsShutdownRequested())
					{
						Presenter->bLoadInProgress = false;
						Presenter->bPendingLoadRequest = false;
						return;
					}

					Presenter->CompleteRefreshRequest(LoadedSnapshot, true);
				});
		});
	FBlueprintHelperMetricsPanelAsyncUtils::TrackTask(MoveTemp(LoadTask));
	return FReply::Handled();
}

void FBlueprintHelperMetricsPanelPresenter::ApplySnapshotAndEmit(
	const FBlueprintHelperMetricsPanelSnapshot& InSnapshot)
{
	Snapshot = InSnapshot;
	EmitCurrentSnapshot();
}

void FBlueprintHelperMetricsPanelPresenter::EmitCurrentSnapshot() const
{
	if (!EventSink)
	{
		return;
	}

	FBlueprintHelperMetricsPanelPresenterEvent Event;
	Event.Snapshot = Snapshot;
	Event.bRefreshView = true;
	EventSink(Event);
}

void FBlueprintHelperMetricsPanelPresenter::QueuePendingRefreshRequest(
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	bPendingLoadRequest = true;
	PendingSelection = Selection;
	ApplySnapshotAndEmit(BuildLoadingSnapshot(Selection));
}

bool FBlueprintHelperMetricsPanelPresenter::ConsumePendingRefreshRequest(
	FBlueprintHelperMetricsPanelSelection& OutSelection)
{
	if (!bPendingLoadRequest)
	{
		return false;
	}

	bPendingLoadRequest = false;
	OutSelection = PendingSelection;
	return true;
}

void FBlueprintHelperMetricsPanelPresenter::CompleteRefreshRequest(
	const FBlueprintHelperMetricsPanelSnapshot& LoadedSnapshot,
	bool bUseAsync)
{
	bLoadInProgress = false;

	FBlueprintHelperMetricsPanelSelection PendingSelectionToLoad;
	if (ConsumePendingRefreshRequest(PendingSelectionToLoad))
	{
		HandleRefreshRequest(PendingSelectionToLoad, bUseAsync);
		return;
	}

	ApplySnapshotAndEmit(LoadedSnapshot);
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildLoadingSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	FBlueprintHelperMetricsPanelSnapshot LoadingSnapshot;
	LoadingSnapshot.TimelineMode = Selection.TimelineMode;
	LoadingSnapshot.Selection = Selection;
	LoadingSnapshot.LoadState = EBlueprintHelperMetricsLoadState::Loading;
	LoadingSnapshot.MetricsRoot = Snapshot.MetricsRoot;
	LoadingSnapshot.StatusText = BlueprintHelperMetricsPanelLoadingStatusText();
	return LoadingSnapshot;
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildErrorSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection,
	const FString& ErrorText) const
{
	FBlueprintHelperMetricsPanelSnapshot ErrorSnapshot;
	ErrorSnapshot.TimelineMode = Selection.TimelineMode;
	ErrorSnapshot.Selection = Selection;
	ErrorSnapshot.LoadState = EBlueprintHelperMetricsLoadState::Error;
	ErrorSnapshot.MetricsRoot = Snapshot.MetricsRoot;
	ErrorSnapshot.ErrorText = ErrorText;
	ErrorSnapshot.StatusText = ErrorText;
	return ErrorSnapshot;
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::LoadSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	return LoadSnapshotCallback
		? LoadSnapshotCallback(Selection)
		: BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelMissingLoaderText());
}
