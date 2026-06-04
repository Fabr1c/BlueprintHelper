// BlueprintHelper Metrics panel presenter implementation.

#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"

#include "Async/Async.h"
#include "Systems/Metrics/BlueprintHelperMetricsStoreReader.h"
#include "Systems/Metrics/BlueprintHelperMetricsTimeSeriesService.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"

static FBlueprintHelperMetricsLoadResult
BlueprintHelperMetricsPanelLoadDefaultMetricsResult()
{
	return FBlueprintHelperMetricsStoreReader::LoadDefault();
}

static FBlueprintHelperMetricsQuery BlueprintHelperMetricsPanelBuildQuery(
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	FBlueprintHelperMetricsQuery Query;
	Query.TimelineMode = Selection.TimelineMode;
	Query.MetricKind = Selection.MetricKind;
	Query.SelectedBucketId = Selection.SelectedBucketId;
	Query.NowUtc = FDateTime::UtcNow();
	return Query;
}

static FBlueprintHelperMetricsPanelSnapshot
BlueprintHelperMetricsPanelBuildSnapshotFromLoadResult(
	const FBlueprintHelperMetricsLoadResult& LoadResult,
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	return FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(
		LoadResult,
		BlueprintHelperMetricsPanelBuildQuery(Selection));
}

static FString BlueprintHelperMetricsPanelLoadingStatusText()
{
	return TEXT("Loading Metrics data...");
}

static FString BlueprintHelperMetricsPanelRefreshingStatusText()
{
	return TEXT("Refreshing Metrics data...");
}

static FString BlueprintHelperMetricsPanelMissingLoaderText()
{
	return TEXT("Metrics load callback is not configured");
}

static FString BlueprintHelperMetricsPanelShutdownText()
{
	return TEXT("Metrics refresh skipped: worker is shutting down");
}

static FString BlueprintHelperMetricsPanelMetricLabel(
	EBlueprintHelperMetricsMetricKind MetricKind)
{
	switch (MetricKind)
	{
	case EBlueprintHelperMetricsMetricKind::ToolUsage:
		return TEXT("Tool Calls");
	case EBlueprintHelperMetricsMetricKind::TaskSpecAttempts:
		return TEXT("TaskSpec Attempts");
	case EBlueprintHelperMetricsMetricKind::ErrorCategories:
		return TEXT("Errors");
	case EBlueprintHelperMetricsMetricKind::TopErrors:
		return TEXT("Top Error Codes");
	case EBlueprintHelperMetricsMetricKind::OperationUsage:
		return TEXT("Operations");
	case EBlueprintHelperMetricsMetricKind::CliIoUsage:
		return TEXT("CLI IO");
	case EBlueprintHelperMetricsMetricKind::EstimatedTokens:
		return TEXT("Estimated Tokens");
	default:
		return TEXT("Tool Calls");
	}
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

static EBlueprintHelperMetricsPanelUpdateScope
BlueprintHelperMetricsPanelScopeFromEvent(
	EBlueprintHelperMetricsVisualEventType EventType)
{
	switch (EventType)
	{
	case EBlueprintHelperMetricsVisualEventType::MetricSelected:
		return EBlueprintHelperMetricsPanelUpdateScope::AllRegions;
	case EBlueprintHelperMetricsVisualEventType::TimelineModeChanged:
	case EBlueprintHelperMetricsVisualEventType::OverviewBucketSelected:
		return EBlueprintHelperMetricsPanelUpdateScope::OverviewAndDetail;
	case EBlueprintHelperMetricsVisualEventType::RefreshClicked:
	default:
		return EBlueprintHelperMetricsPanelUpdateScope::AllRegions;
	}
}

static FString BlueprintHelperMetricsPanelResolveSelectedBucketId(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot,
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	if (!Selection.SelectedBucketId.IsEmpty())
	{
		return Selection.SelectedBucketId;
	}
	return Snapshot.OverviewBars.Num() > 0
		? Snapshot.OverviewBars.Last().BucketId
		: FString();
}

static FBlueprintHelperMetricsPanelSnapshot
BlueprintHelperMetricsPanelProjectSnapshot(
	const FBlueprintHelperMetricsPanelSnapshot& BaseSnapshot,
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	FBlueprintHelperMetricsPanelSnapshot ProjectedSnapshot = BaseSnapshot;
	ProjectedSnapshot.TimelineMode = Selection.TimelineMode;
	ProjectedSnapshot.Selection = Selection;
	ProjectedSnapshot.Selection.SelectedBucketId =
		BlueprintHelperMetricsPanelResolveSelectedBucketId(BaseSnapshot, Selection);
	ProjectedSnapshot.SelectedMetricTitle =
		BlueprintHelperMetricsPanelMetricLabel(Selection.MetricKind);
	ProjectedSnapshot.bRefreshInProgress = false;
	ProjectedSnapshot.RefreshStatusText.Reset();

	for (FBlueprintHelperMetricsMetricOptionView& Option :
		ProjectedSnapshot.MetricOptions)
	{
		Option.bIsSelected = Option.Kind == Selection.MetricKind;
	}

	ProjectedSnapshot.SelectedBucketLabel.Reset();
	ProjectedSnapshot.SelectedBucketTotal = 0;
	for (FBlueprintHelperMetricsOverviewBarView& Bar :
		ProjectedSnapshot.OverviewBars)
	{
		Bar.bIsSelected =
			Bar.BucketId == ProjectedSnapshot.Selection.SelectedBucketId;
		if (Bar.bIsSelected)
		{
			ProjectedSnapshot.SelectedBucketLabel = Bar.Label;
			ProjectedSnapshot.SelectedBucketTotal = Bar.Value;
		}
	}

	return ProjectedSnapshot;
}

TSharedRef<FBlueprintHelperMetricsPanelPresenter>
FBlueprintHelperMetricsPanelPresenter::CreateDefault()
{
	return MakeShared<FBlueprintHelperMetricsPanelPresenter>();
}

FBlueprintHelperMetricsPanelPresenter::FBlueprintHelperMetricsPanelPresenter()
	: LoadMetricsResultCallback(&BlueprintHelperMetricsPanelLoadDefaultMetricsResult)
{
}

FBlueprintHelperMetricsPanelPresenter::FBlueprintHelperMetricsPanelPresenter(
	FLoadSnapshot InLoadSnapshot)
	: LoadSnapshotCallback(MoveTemp(InLoadSnapshot))
{
}

FBlueprintHelperMetricsPanelPresenter::FBlueprintHelperMetricsPanelPresenter(
	FLoadMetricsResult InLoadMetricsResult)
	: LoadMetricsResultCallback(MoveTemp(InLoadMetricsResult))
{
}

void FBlueprintHelperMetricsPanelPresenter::SetEventSink(
	FPresenterEventSink InEventSink)
{
	EventSink = MoveTemp(InEventSink);
	EmitCurrentSnapshot(EBlueprintHelperMetricsPanelUpdateScope::InitialContent);
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleVisualEvent(
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	const FBlueprintHelperMetricsPanelSelection NextSelection =
		BlueprintHelperMetricsPanelSelectionFromEvent(Snapshot.Selection, Event);
	if (Event.Type == EBlueprintHelperMetricsVisualEventType::RefreshClicked)
	{
		return HandleRefreshRequest(NextSelection, true);
	}

	return HandleCachedSelectionRequest(
		NextSelection,
		BlueprintHelperMetricsPanelScopeFromEvent(Event.Type),
		true);
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleVisualEventForTests(
	const FBlueprintHelperMetricsPanelVisualEvent& Event)
{
	const FBlueprintHelperMetricsPanelSelection NextSelection =
		BlueprintHelperMetricsPanelSelectionFromEvent(Snapshot.Selection, Event);
	if (Event.Type == EBlueprintHelperMetricsVisualEventType::RefreshClicked)
	{
		return HandleRefreshRequest(NextSelection, false);
	}

	return HandleCachedSelectionRequest(
		NextSelection,
		BlueprintHelperMetricsPanelScopeFromEvent(Event.Type),
		false);
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
		const EBlueprintHelperMetricsPanelUpdateScope Scope =
			Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded
				? EBlueprintHelperMetricsPanelUpdateScope::StatusOnly
				: EBlueprintHelperMetricsPanelUpdateScope::InitialContent;
		ApplySnapshotAndEmit(BuildRefreshingSnapshotFromCurrent(Selection), Scope);
		return FReply::Handled();
	}

	if (bUseAsync &&
		FBlueprintHelperMetricsPanelAsyncUtils::IsShutdownRequested())
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelShutdownText()),
			EBlueprintHelperMetricsPanelUpdateScope::Error);
		return FReply::Handled();
	}

	if (!LoadSnapshotCallback && !LoadMetricsResultCallback)
	{
		ApplySnapshotAndEmit(
			BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelMissingLoaderText()),
			EBlueprintHelperMetricsPanelUpdateScope::Error);
		return FReply::Handled();
	}

	bLoadInProgress = true;
	const EBlueprintHelperMetricsPanelUpdateScope RefreshScope =
		Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded
			? EBlueprintHelperMetricsPanelUpdateScope::StatusOnly
			: EBlueprintHelperMetricsPanelUpdateScope::InitialContent;
	ApplySnapshotAndEmit(
		BuildRefreshingSnapshotFromCurrent(Selection),
		RefreshScope);

	if (!bUseAsync)
	{
		const FLoadCompletion Completion = LoadSnapshot(Selection);
		CompleteRefreshRequest(Completion, false);
		return FReply::Handled();
	}

	const FBlueprintHelperMetricsPanelSelection RequestedSelection = Selection;
	TWeakPtr<FBlueprintHelperMetricsPanelPresenter> WeakPresenter = AsShared();
	const FLoadSnapshot SnapshotLoader = LoadSnapshotCallback;
	const FLoadMetricsResult MetricsResultLoader = LoadMetricsResultCallback;
	TFuture<void> LoadTask = Async(
		EAsyncExecution::ThreadPool,
		[
			WeakPresenter,
			SnapshotLoader,
			MetricsResultLoader,
			RequestedSelection
		]()
		{
			FLoadCompletion Completion;
			if (MetricsResultLoader)
			{
				Completion.LoadResult = MetricsResultLoader();
				Completion.bHasLoadResult = true;
				Completion.Snapshot =
					BlueprintHelperMetricsPanelBuildSnapshotFromLoadResult(
						Completion.LoadResult,
						RequestedSelection);
			}
			else if (SnapshotLoader)
			{
				Completion.Snapshot = SnapshotLoader(RequestedSelection);
			}

			AsyncTask(
				ENamedThreads::GameThread,
				[WeakPresenter, Completion]() mutable
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

					Presenter->CompleteRefreshRequest(Completion, true);
				});
		});
	FBlueprintHelperMetricsPanelAsyncUtils::TrackTask(MoveTemp(LoadTask));
	return FReply::Handled();
}

FReply FBlueprintHelperMetricsPanelPresenter::HandleCachedSelectionRequest(
	const FBlueprintHelperMetricsPanelSelection& Selection,
	EBlueprintHelperMetricsPanelUpdateScope UpdateScope,
	bool bUseAsync)
{
	if (!CanProjectFromCache())
	{
		return HandleRefreshRequest(Selection, bUseAsync);
	}

	if (bLoadInProgress)
	{
		QueuePendingRefreshRequest(Selection);
	}

	FBlueprintHelperMetricsPanelSnapshot ProjectedSnapshot =
		BuildSnapshotFromCachedProjection(Selection);
	if (Snapshot.bRefreshInProgress)
	{
		ProjectedSnapshot.bRefreshInProgress = true;
		ProjectedSnapshot.RefreshStatusText =
			BlueprintHelperMetricsPanelRefreshingStatusText();
	}
	ApplySnapshotAndEmit(ProjectedSnapshot, UpdateScope);
	return FReply::Handled();
}

void FBlueprintHelperMetricsPanelPresenter::ApplySnapshotAndEmit(
	const FBlueprintHelperMetricsPanelSnapshot& InSnapshot,
	EBlueprintHelperMetricsPanelUpdateScope UpdateScope)
{
	Snapshot = InSnapshot;
	EmitCurrentSnapshot(UpdateScope);
}

void FBlueprintHelperMetricsPanelPresenter::EmitCurrentSnapshot(
	EBlueprintHelperMetricsPanelUpdateScope UpdateScope) const
{
	if (!EventSink)
	{
		return;
	}

	FBlueprintHelperMetricsPanelPresenterEvent Event;
	Event.Snapshot = Snapshot;
	Event.UpdateScope = UpdateScope;
	Event.bRefreshView = true;
	EventSink(Event);
}

void FBlueprintHelperMetricsPanelPresenter::QueuePendingRefreshRequest(
	const FBlueprintHelperMetricsPanelSelection& Selection)
{
	bPendingLoadRequest = true;
	PendingSelection = Selection;
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
	const FLoadCompletion& Completion,
	bool bUseAsync)
{
	const EBlueprintHelperMetricsPanelUpdateScope CompletionScope =
		Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded
			? EBlueprintHelperMetricsPanelUpdateScope::AllRegions
			: EBlueprintHelperMetricsPanelUpdateScope::InitialContent;

	bLoadInProgress = false;
	CacheLoadCompletion(Completion);

	FBlueprintHelperMetricsPanelSelection PendingSelectionToProject;
	if (ConsumePendingRefreshRequest(PendingSelectionToProject))
	{
		FBlueprintHelperMetricsPanelSnapshot PendingSnapshot;
		if (bCachedLoadResultAvailable)
		{
			PendingSnapshot =
				BuildSnapshotFromCachedProjection(PendingSelectionToProject);
		}
		else if (Completion.Snapshot.LoadState ==
			EBlueprintHelperMetricsLoadState::Loaded)
		{
			PendingSnapshot = BlueprintHelperMetricsPanelProjectSnapshot(
				Completion.Snapshot,
				PendingSelectionToProject);
		}
		else
		{
			ApplySnapshotAndEmit(
				Completion.Snapshot,
				CompletionScope);
			HandleRefreshRequest(PendingSelectionToProject, bUseAsync);
			return;
		}

		ApplySnapshotAndEmit(
			PendingSnapshot,
			CompletionScope);
		return;
	}

	ApplySnapshotAndEmit(
		Completion.Snapshot,
		CompletionScope);
}

void FBlueprintHelperMetricsPanelPresenter::CacheLoadCompletion(
	const FLoadCompletion& Completion)
{
	if (Completion.bHasLoadResult)
	{
		CachedLoadResult = Completion.LoadResult;
		bCachedLoadResultAvailable = true;
	}
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
	LoadingSnapshot.bRefreshInProgress = true;
	LoadingSnapshot.RefreshStatusText =
		BlueprintHelperMetricsPanelLoadingStatusText();
	return LoadingSnapshot;
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildRefreshingSnapshotFromCurrent(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	if (Snapshot.LoadState != EBlueprintHelperMetricsLoadState::Loaded)
	{
		return BuildLoadingSnapshot(Selection);
	}

	FBlueprintHelperMetricsPanelSnapshot RefreshingSnapshot = Snapshot;
	RefreshingSnapshot.TimelineMode = Selection.TimelineMode;
	RefreshingSnapshot.Selection = Selection;
	RefreshingSnapshot.LoadState = EBlueprintHelperMetricsLoadState::Loaded;
	RefreshingSnapshot.bRefreshInProgress = true;
	RefreshingSnapshot.RefreshStatusText =
		BlueprintHelperMetricsPanelRefreshingStatusText();
	return RefreshingSnapshot;
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
FBlueprintHelperMetricsPanelPresenter::BuildSnapshotFromCachedProjection(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	if (bCachedLoadResultAvailable)
	{
		return BuildSnapshotFromLoadResult(CachedLoadResult, Selection);
	}
	return BlueprintHelperMetricsPanelProjectSnapshot(Snapshot, Selection);
}

FBlueprintHelperMetricsPanelSnapshot
FBlueprintHelperMetricsPanelPresenter::BuildSnapshotFromLoadResult(
	const FBlueprintHelperMetricsLoadResult& LoadResult,
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	return FBlueprintHelperMetricsTimeSeriesService::BuildSnapshot(
		LoadResult,
		BuildQuery(Selection));
}

FBlueprintHelperMetricsQuery FBlueprintHelperMetricsPanelPresenter::BuildQuery(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	return BlueprintHelperMetricsPanelBuildQuery(Selection);
}

FBlueprintHelperMetricsPanelPresenter::FLoadCompletion
FBlueprintHelperMetricsPanelPresenter::LoadSnapshot(
	const FBlueprintHelperMetricsPanelSelection& Selection) const
{
	FLoadCompletion Completion;
	if (LoadMetricsResultCallback)
	{
		Completion.LoadResult = LoadMetricsResultCallback();
		Completion.bHasLoadResult = true;
		Completion.Snapshot =
			BuildSnapshotFromLoadResult(Completion.LoadResult, Selection);
		return Completion;
	}
	if (LoadSnapshotCallback)
	{
		Completion.Snapshot = LoadSnapshotCallback(Selection);
		return Completion;
	}

	Completion.Snapshot =
		BuildErrorSnapshot(Selection, BlueprintHelperMetricsPanelMissingLoaderText());
	return Completion;
}

bool FBlueprintHelperMetricsPanelPresenter::CanProjectFromCache() const
{
	return bCachedLoadResultAvailable ||
		Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded;
}
