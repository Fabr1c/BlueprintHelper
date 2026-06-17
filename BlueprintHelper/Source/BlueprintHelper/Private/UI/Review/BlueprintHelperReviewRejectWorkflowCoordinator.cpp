// BlueprintHelper Review reject workflow coordinator implementation.

#include "UI/Review/BlueprintHelperReviewRejectWorkflowCoordinator.h"

#include "Async/Async.h"
#include "UI/Review/Utils/BlueprintHelperReviewPanelAsyncUtils.h"
#include "UI/Review/Utils/BlueprintHelperReviewPanelLocalUtils.h"

void FBlueprintHelperReviewRejectWorkflowCoordinator::SetCallbacks(FCallbacks InCallbacks)
{
	Callbacks = MoveTemp(InCallbacks);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::EnqueueReject(const FString& ChangeId)
{
	WorkflowModel.EnqueueReject(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::StartNextPrepare()
{
	if (WorkflowModel.IsPrepareActive())
	{
		return;
	}

	FString ChangeId;
	while (WorkflowModel.TryPopNextPending(ChangeId))
	{
		if (!Callbacks.ResolveChangeSnapshot)
		{
			return;
		}

		const TOptional<FBlueprintHelperReviewVisibleChange> ChangeSnapshot =
			Callbacks.ResolveChangeSnapshot(ChangeId);
		if (!ChangeSnapshot.IsSet())
		{
			if (Callbacks.OnChangeMissing)
			{
				Callbacks.OnChangeMissing(ChangeId);
			}
			WorkflowModel.FinishReject(ChangeId);
			continue;
		}

		WorkflowModel.MarkPrepareStarted(ChangeId);
		if (Callbacks.OnPrepareStarted)
		{
			Callbacks.OnPrepareStarted(ChangeId, ChangeSnapshot.GetValue());
		}

		const TWeakPtr<FBlueprintHelperReviewRejectWorkflowCoordinator> WeakCoordinator = AsShared();
		TFuture<void> PrepareTask = Async(EAsyncExecution::ThreadPool, [WeakCoordinator, ChangeId, ChangeSnapshot]()
		{
			FBlueprintHelperReviewRejectOptions PreparedOptions =
				FBlueprintHelperReviewPanelLocalUtils::PrepareRejectOptions(ChangeSnapshot.GetValue());
			if (FBlueprintHelperReviewPanelAsyncUtils::IsShutdownRequested())
			{
				return;
			}
			AsyncTask(ENamedThreads::GameThread, [WeakCoordinator, ChangeId, PreparedOptions]()
			{
				if (TSharedPtr<FBlueprintHelperReviewRejectWorkflowCoordinator> Coordinator = WeakCoordinator.Pin())
				{
					Coordinator->HandlePreparedRejectReady(ChangeId, PreparedOptions);
				}
			});
		});
		FBlueprintHelperReviewPanelAsyncUtils::TrackTask(MoveTemp(PrepareTask));
		return;
	}
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::HandlePreparedRejectReady(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
	WorkflowModel.MarkPrepareFinished(ChangeId, PreparedOptions);
	if (Callbacks.OnPrepareFinished)
	{
		Callbacks.OnPrepareFinished(ChangeId, PreparedOptions);
	}
	if (Callbacks.ExecutePreparedMutation)
	{
		Callbacks.ExecutePreparedMutation(ChangeId);
	}
}

const FBlueprintHelperReviewRejectOptions* FBlueprintHelperReviewRejectWorkflowCoordinator::FindPreparedOptions(
	const FString& ChangeId) const
{
	return WorkflowModel.FindPreparedOptions(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::FinishReject(const FString& ChangeId)
{
	WorkflowModel.FinishReject(ChangeId);
	if (WorkflowModel.HasPendingRejects())
	{
		StartNextPrepare();
	}
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::BeginTiming(
	const FBlueprintHelperReviewVisibleChange& Change,
	double NowSeconds)
{
	TimingModel.Begin(Change, NowSeconds);
}

bool FBlueprintHelperReviewRejectWorkflowCoordinator::ContainsTiming(const FString& ChangeId) const
{
	return TimingModel.Contains(ChangeId);
}

bool FBlueprintHelperReviewRejectWorkflowCoordinator::IsWaitingForStoreRefresh(const FString& ChangeId) const
{
	return TimingModel.IsWaitingForStoreRefresh(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::MarkWaitingForStoreRefresh(const FString& ChangeId)
{
	TimingModel.MarkWaitingForStoreRefresh(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::CancelWaitingForStoreRefresh(const FString& ChangeId)
{
	TimingModel.CancelWaitingForStoreRefresh(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowCoordinator::CompleteTiming(const FString& ChangeId)
{
	TimingModel.Complete(ChangeId);
}

FBlueprintHelperReviewRejectTimingSample FBlueprintHelperReviewRejectWorkflowCoordinator::RecordStage(
	const FString& ChangeId,
	const FString& Stage,
	double NowSeconds,
	const FString& Detail)
{
	return TimingModel.RecordStage(ChangeId, Stage, NowSeconds, Detail);
}

TArray<FBlueprintHelperReviewRejectTimingSample>
FBlueprintHelperReviewRejectWorkflowCoordinator::RecordMatchingStoreEvent(
	const FBlueprintHelperReviewStoreChangedEvent& Event,
	const FString& Stage,
	double NowSeconds,
	const FString& Detail,
	bool bCompleteMatches)
{
	return TimingModel.RecordMatchingStoreEvent(
		Event,
		Stage,
		NowSeconds,
		Detail,
		bCompleteMatches);
}
