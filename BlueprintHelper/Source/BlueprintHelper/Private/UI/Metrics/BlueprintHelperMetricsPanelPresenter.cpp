// BlueprintHelper Metrics panel presenter implementation.

#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"

#include "Async/Async.h"
#include "Systems/Metrics/BlueprintHelperMetricsStoreReader.h"
#include "Systems/Metrics/BlueprintHelperMetricsTimeSeriesService.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"

static FBlueprintHelperMetricsPanelSnapshot
BlueprintHelperMetricsPanelLoadDefaultSnapshot(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	const FBlueprintHelperMetricsLoadResult LoadResult =
		FBlueprintHelperMetricsStoreReader::LoadDefault();

	FBlueprintHelperMetricsQuery Query;
	Query.TimelineMode = TimelineMode;
	Query.NowUtc = FDateTime::UtcNow();
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
	switch (Event.Type)
	{
	case EBlueprintHelperMetricsVisualEventType::RefreshClicked:
		return HandleRefreshRequest(Snapshot.TimelineMode, true);
	case EBlueprintHelperMetricsVisualEventType::TimelineModeChanged:
		return HandleRefreshRequest(Event.TimelineMode, true);
	default:
		return FReply::Handled();
	}
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleVisualEventForTests(
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	switch (Event.Type)
	{
	case EBlueprintHelperMetricsVisualEventType::RefreshClicked:
		return HandleRefreshRequest(Snapshot.TimelineMode, false);
	case EBlueprintHelperMetricsVisualEventType::TimelineModeChanged:
		return HandleRefreshRequest(Event.TimelineMode, false);
	default:
		return FReply::Handled();
	}
}

const FBlueprintHelperMetricsPanelSnapshot&
FBlueprintHelperMetricsPanelPresenter::GetSnapshot() const
{
	return Snapshot;
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleRefreshRequest(
	EBlueprintHelperMetricsTimelineMode TimelineMode,
	bool bUseAsync)
{
	if (bLoadInProgress)
	{
		QueuePendingRefreshRequest(TimelineMode);
		return FReply::Handled();
	}

	if (bUseAsync &&
		FBlueprintHelperMetricsPanelAsyncUtils::IsShutdownRequested())
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(TimelineMode, BlueprintHelperMetricsPanelShutdownText()));
		return FReply::Handled();
	}

	if (!LoadSnapshotCallback)
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(TimelineMode, BlueprintHelperMetricsPanelMissingLoaderText()));
		return FReply::Handled();
	}

	bLoadInProgress = true;
	ApplySnapshotAndEmit(BuildLoadingSnapshot(TimelineMode));

	if (!bUseAsync)
	{
		const FBlueprintHelperMetricsPanelSnapshot LoadedSnapshot =
			LoadSnapshot(TimelineMode);
		CompleteRefreshRequest(LoadedSnapshot, false);
		return FReply::Handled();
	}

	TWeakPtr<FBlueprintHelperMetricsPanelPresenter> WeakPresenter = AsShared();
	const FLoadSnapshot Loader = LoadSnapshotCallback;
	TFuture<void> LoadTask = Async(
		EAsyncExecution::ThreadPool,
		[WeakPresenter, Loader, TimelineMode]()
		{
			FBlueprintHelperMetricsPanelSnapshot LoadedSnapshot;
			if (Loader)
			{
				LoadedSnapshot = Loader(TimelineMode);
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
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	bPendingLoadRequest = true;
	PendingTimelineMode = TimelineMode;
	ApplySnapshotAndEmit(BuildLoadingSnapshot(TimelineMode));
}

bool FBlueprintHelperMetricsPanelPresenter::ConsumePendingRefreshRequest(
	EBlueprintHelperMetricsTimelineMode& OutTimelineMode)
{
	if (!bPendingLoadRequest)
	{
		return false;
	}

	bPendingLoadRequest = false;
	OutTimelineMode = PendingTimelineMode;
	return true;
}

void FBlueprintHelperMetricsPanelPresenter::CompleteRefreshRequest(
	const FBlueprintHelperMetricsPanelSnapshot& LoadedSnapshot,
	bool bUseAsync)
{
	bLoadInProgress = false;

	EBlueprintHelperMetricsTimelineMode PendingMode =
		EBlueprintHelperMetricsTimelineMode::Daily;
	if (ConsumePendingRefreshRequest(PendingMode))
	{
		HandleRefreshRequest(PendingMode, bUseAsync);
		return;
	}

	ApplySnapshotAndEmit(LoadedSnapshot);
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildLoadingSnapshot(
	EBlueprintHelperMetricsTimelineMode TimelineMode) const
{
	FBlueprintHelperMetricsPanelSnapshot LoadingSnapshot;
	LoadingSnapshot.TimelineMode = TimelineMode;
	LoadingSnapshot.LoadState = EBlueprintHelperMetricsLoadState::Loading;
	LoadingSnapshot.MetricsRoot = Snapshot.MetricsRoot;
	LoadingSnapshot.StatusText = BlueprintHelperMetricsPanelLoadingStatusText();
	return LoadingSnapshot;
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildErrorSnapshot(
	EBlueprintHelperMetricsTimelineMode TimelineMode,
	const FString& ErrorText) const
{
	FBlueprintHelperMetricsPanelSnapshot ErrorSnapshot;
	ErrorSnapshot.TimelineMode = TimelineMode;
	ErrorSnapshot.LoadState = EBlueprintHelperMetricsLoadState::Error;
	ErrorSnapshot.MetricsRoot = Snapshot.MetricsRoot;
	ErrorSnapshot.ErrorText = ErrorText;
	ErrorSnapshot.StatusText = ErrorText;
	return ErrorSnapshot;
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::LoadSnapshot(
	EBlueprintHelperMetricsTimelineMode TimelineMode) const
{
	return LoadSnapshotCallback
		? LoadSnapshotCallback(TimelineMode)
		: BuildErrorSnapshot(TimelineMode, BlueprintHelperMetricsPanelMissingLoaderText());
}
