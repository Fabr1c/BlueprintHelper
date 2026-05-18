// BlueprintHelper main window presenter event bridge implementation.

#include "UI/BlueprintHelperMainWindowPresenter.h"

#include "Async/Async.h"
#include "Systems/Review/BlueprintHelperReviewedDataCleanupService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/Utils/BlueprintHelperMainWindowCleanupAsyncUtils.h"

FBlueprintHelperMainWindowVisualEvent FBlueprintHelperMainWindowVisualEvent::CleanupReviewDataClicked()
{
	FBlueprintHelperMainWindowVisualEvent Event;
	Event.Type = EBlueprintHelperMainWindowVisualEventType::CleanupReviewDataClicked;
	return Event;
}

FBlueprintHelperMainWindowPresenterEvent FBlueprintHelperMainWindowPresenterEvent::ShowCleanupNotification(
	const FString& InStatusText)
{
	FBlueprintHelperMainWindowPresenterEvent Event;
	Event.bShowCleanupNotification = true;
	Event.CleanupStatusText = InStatusText;
	return Event;
}

FBlueprintHelperMainWindowPresenterEvent FBlueprintHelperMainWindowPresenterEvent::UpdateCleanupNotification(
	const FString& InStatusText,
	bool bInSucceeded,
	bool bInExpire)
{
	FBlueprintHelperMainWindowPresenterEvent Event;
	Event.bUpdateCleanupNotification = true;
	Event.bCleanupSucceeded = bInSucceeded;
	Event.bExpireCleanupNotification = bInExpire;
	Event.CleanupStatusText = InStatusText;
	return Event;
}

FBlueprintHelperMainWindowPresenterEvent FBlueprintHelperMainWindowPresenterEvent::LogStatus(
	const FString& InLogMessage,
	bool bInLogAsWarning)
{
	FBlueprintHelperMainWindowPresenterEvent Event;
	Event.LogMessage = InLogMessage;
	Event.bLogAsWarning = bInLogAsWarning;
	return Event;
}

FBlueprintHelperMainWindowPresenter::FBlueprintHelperMainWindowPresenter(
	const FBlueprintHelperReviewStoreService* InReviewStoreService)
	: ReviewStoreService(InReviewStoreService)
{
}

void FBlueprintHelperMainWindowPresenter::SetEventSink(FPresenterEventSink InEventSink)
{
	EventSink = MoveTemp(InEventSink);
}

FReply FBlueprintHelperMainWindowPresenter::HandleVisualEvent(
	const FBlueprintHelperMainWindowVisualEvent& Event)
{
	if (Event.Type == EBlueprintHelperMainWindowVisualEventType::CleanupReviewDataClicked)
	{
		return HandleCleanupReviewDataClicked();
	}
	return FReply::Handled();
}

const FString& FBlueprintHelperMainWindowPresenter::GetLastCleanupStatus() const
{
	return LastCleanupStatus;
}

FReply FBlueprintHelperMainWindowPresenter::HandleCleanupReviewDataClicked()
{
	if (bCleanupInProgress)
	{
		return FReply::Handled();
	}
	if (FBlueprintHelperMainWindowCleanupAsyncUtils::IsShutdownRequested())
	{
		LastCleanupStatus = TEXT("CleanReviewData skipped: cleanup worker is shutting down");
		EmitLogStatus(LastCleanupStatus, true);
		return FReply::Handled();
	}

	bCleanupInProgress = true;
	LastCleanupStatus = TEXT("CleanReviewData scanning...");
	EmitEvent(FBlueprintHelperMainWindowPresenterEvent::ShowCleanupNotification(
		TEXT("\u626b\u63cf\u4e2d")));

	TWeakPtr<FBlueprintHelperMainWindowPresenter> WeakPresenter = AsShared();
	TFuture<void> ScanTask = Async(EAsyncExecution::ThreadPool, [WeakPresenter]()
	{
		const FBlueprintHelperReviewedDataCleanupPlan Plan =
			FBlueprintHelperReviewedDataCleanupService::ScanCleanupPlan();
		AsyncTask(ENamedThreads::GameThread, [WeakPresenter, Plan]()
		{
			TSharedPtr<FBlueprintHelperMainWindowPresenter> Presenter = WeakPresenter.Pin();
			if (!Presenter.IsValid())
			{
				return;
			}
			if (FBlueprintHelperMainWindowCleanupAsyncUtils::IsShutdownRequested())
			{
				Presenter->bCleanupInProgress = false;
				Presenter->LastCleanupStatus =
					TEXT("CleanReviewData skipped: cleanup worker is shutting down");
				Presenter->EmitEvent(FBlueprintHelperMainWindowPresenterEvent::UpdateCleanupNotification(
					TEXT("\u5931\u8d25"),
					false,
					true));
				Presenter->EmitLogStatus(Presenter->LastCleanupStatus, true);
				return;
			}

			Presenter->LastCleanupStatus = FString::Printf(
				TEXT("CleanReviewData cleaning... recordsScanned=%d reviewedChangesToRemove=%d filesToDelete=%d"),
				Plan.RecordsScanned,
				Plan.ChangesRemoved,
					+ Plan.SessionFilePathsToDelete.Num()
					+ Plan.OldDebugBundlePathsToDelete.Num()
					+ Plan.CompletedDebugBundlePathsToDelete.Num());
			Presenter->EmitEvent(FBlueprintHelperMainWindowPresenterEvent::UpdateCleanupNotification(
				TEXT("\u6e05\u7406\u4e2d"),
				true,
				false));
			Presenter->EmitLogStatus(Presenter->LastCleanupStatus);

			TWeakPtr<FBlueprintHelperMainWindowPresenter> CleanupWeakPresenter = Presenter;
			TFuture<void> CleanupTask = Async(EAsyncExecution::ThreadPool, [CleanupWeakPresenter, Plan]()
			{
				const FBlueprintHelperReviewedDataCleanupResult Result =
					FBlueprintHelperReviewedDataCleanupService::ExecuteCleanupPlan(Plan);
				AsyncTask(ENamedThreads::GameThread, [CleanupWeakPresenter, Result]()
				{
					TSharedPtr<FBlueprintHelperMainWindowPresenter> CleanupPresenter =
						CleanupWeakPresenter.Pin();
					if (!CleanupPresenter.IsValid())
					{
						return;
					}

					CleanupPresenter->bCleanupInProgress = false;
					CleanupPresenter->LastCleanupStatus = FString::Printf(
						TEXT("CleanReviewData reviewedChangesRemoved=%d recordsSaved=%d recordsDeleted=%d sessionFilesDeleted=%d oldDebugBundlesDeleted=%d completedDebugBundlesDeleted=%d failures=%d error=\"%s\""),
						Result.ChangesRemoved,
						Result.RecordsSaved,
						Result.RecordsDeleted,
						Result.SessionFilesDeleted,
						Result.OldDebugBundlesDeleted,
						Result.CompletedDebugBundlesDeleted,
						Result.Failures,
						*Result.Error);
					CleanupPresenter->EmitEvent(FBlueprintHelperMainWindowPresenterEvent::UpdateCleanupNotification(
						Result.Failures == 0 ? TEXT("\u5b8c\u6210") : TEXT("\u5931\u8d25"),
						Result.Failures == 0,
						true));
					CleanupPresenter->EmitLogStatus(CleanupPresenter->LastCleanupStatus);
					if (CleanupPresenter->ReviewStoreService)
					{
						CleanupPresenter->ReviewStoreService->NotifyPendingReviewChanged();
					}
				});
			});
			FBlueprintHelperMainWindowCleanupAsyncUtils::TrackCleanupTask(MoveTemp(CleanupTask));
		});
	});
	FBlueprintHelperMainWindowCleanupAsyncUtils::TrackCleanupTask(MoveTemp(ScanTask));
	return FReply::Handled();
}

void FBlueprintHelperMainWindowPresenter::EmitEvent(
	const FBlueprintHelperMainWindowPresenterEvent& Event) const
{
	if (EventSink)
	{
		EventSink(Event);
	}
}

void FBlueprintHelperMainWindowPresenter::EmitLogStatus(
	const FString& InLogMessage,
	bool bInLogAsWarning) const
{
	EmitEvent(FBlueprintHelperMainWindowPresenterEvent::LogStatus(InLogMessage, bInLogAsWarning));
}
