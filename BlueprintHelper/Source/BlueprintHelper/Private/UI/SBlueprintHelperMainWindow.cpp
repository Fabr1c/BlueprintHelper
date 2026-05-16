// BlueprintHelper main window shell implementation.

#include "UI/SBlueprintHelperMainWindow.h"

#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Systems/Review/BlueprintHelperReviewedDataCleanupService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/SHelperMainWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
static FCriticalSection GBlueprintHelperReviewedDataCleanupTaskCriticalSection;
static TArray<TFuture<void>> GBlueprintHelperReviewedDataCleanupTasks;
static bool bBlueprintHelperReviewedDataCleanupShutdown = false;

static bool IsBlueprintHelperReviewedDataCleanupShutdownRequested()
{
	FScopeLock Lock(&GBlueprintHelperReviewedDataCleanupTaskCriticalSection);
	return bBlueprintHelperReviewedDataCleanupShutdown;
}

static void TrackBlueprintHelperReviewedDataCleanupTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&GBlueprintHelperReviewedDataCleanupTaskCriticalSection);
		for (int32 Index = GBlueprintHelperReviewedDataCleanupTasks.Num() - 1; Index >= 0; --Index)
		{
			if (GBlueprintHelperReviewedDataCleanupTasks[Index].IsReady())
			{
				GBlueprintHelperReviewedDataCleanupTasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
		if (bBlueprintHelperReviewedDataCleanupShutdown)
		{
			bWaitImmediately = true;
		}
		else
		{
			GBlueprintHelperReviewedDataCleanupTasks.Add(MoveTemp(Future));
			return;
		}
	}
	if (bWaitImmediately)
	{
		Future.Wait();
	}
}

static void FlushBlueprintHelperReviewedDataCleanupTasksInternal(bool bShutdown)
{
	TArray<TFuture<void>> Tasks;
	{
		FScopeLock Lock(&GBlueprintHelperReviewedDataCleanupTaskCriticalSection);
		bBlueprintHelperReviewedDataCleanupShutdown = bBlueprintHelperReviewedDataCleanupShutdown || bShutdown;
		Tasks = MoveTemp(GBlueprintHelperReviewedDataCleanupTasks);
	}
	for (TFuture<void>& Task : Tasks)
	{
		Task.Wait();
	}
}
}

void SBlueprintHelperMainWindow::Construct(const FArguments& InArgs)
{
	ImportService = InArgs._ImportService;
	GraphResolver = InArgs._GraphResolver;
	ReviewStoreService = InArgs._ReviewStoreService;
	ReviewActionService = InArgs._ReviewActionService;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetToolsTabColor)
				.Text(FText::FromString(TEXT("Tools")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowToolsPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetReviewTabColor)
				.Text(FText::FromString(TEXT("Review")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowReviewPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Clean Review Data")))
				.ToolTipText(FText::FromString(TEXT("Clean reviewed Accept/Reject residual Review records and unreferenced Transaction files. Pending review data is preserved.")))
				.OnClicked(this, &SBlueprintHelperMainWindow::OnCleanupReviewDataClicked)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(PageSwitcher, SWidgetSwitcher)
			.WidgetIndex(ActivePageIndex)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHelperMainWidget)
				.ImportService(ImportService)
				.GraphResolver(GraphResolver)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperReviewPanel)
				.ReviewStoreService(ReviewStoreService)
				.ReviewActionService(ReviewActionService)
			]
		]
	];
}

void SBlueprintHelperMainWindow::FlushCleanupTasks()
{
	FlushBlueprintHelperReviewedDataCleanupTasksInternal(false);
}

void SBlueprintHelperMainWindow::ShutdownCleanupTasks()
{
	FlushBlueprintHelperReviewedDataCleanupTasksInternal(true);
}

FReply SBlueprintHelperMainWindow::ShowToolsPage()
{
	ActivePageIndex = 0;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::ShowReviewPage()
{
	ActivePageIndex = 1;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::OnCleanupReviewDataClicked()
{
	if (bCleanupInProgress)
	{
		return FReply::Handled();
	}
	if (IsBlueprintHelperReviewedDataCleanupShutdownRequested())
	{
		LastCleanupStatus = TEXT("CleanReviewData skipped: cleanup worker is shutting down");
		UE_LOG(LogTemp, Warning, TEXT("BlueprintHelper %s"), *LastCleanupStatus);
		return FReply::Handled();
	}

	bCleanupInProgress = true;
	LastCleanupStatus = TEXT("CleanReviewData scanning...");
	ShowCleanupNotification(TEXT("扫描中"));
	TWeakPtr<SBlueprintHelperMainWindow> WeakWindow =
		StaticCastSharedRef<SBlueprintHelperMainWindow>(AsShared());
	TFuture<void> ScanTask = Async(EAsyncExecution::ThreadPool, [WeakWindow]()
	{
		const FBlueprintHelperReviewedDataCleanupPlan Plan =
			FBlueprintHelperReviewedDataCleanupService::ScanCleanupPlan();
		AsyncTask(ENamedThreads::GameThread, [WeakWindow, Plan]()
		{
			if (TSharedPtr<SBlueprintHelperMainWindow> Window = WeakWindow.Pin())
			{
				if (IsBlueprintHelperReviewedDataCleanupShutdownRequested())
				{
					Window->bCleanupInProgress = false;
					Window->LastCleanupStatus = TEXT("CleanReviewData skipped: cleanup worker is shutting down");
					Window->UpdateCleanupNotification(TEXT("失败"), false, true);
					UE_LOG(LogTemp, Warning, TEXT("BlueprintHelper %s"), *Window->LastCleanupStatus);
					return;
				}

				Window->LastCleanupStatus = FString::Printf(
					TEXT("CleanReviewData cleaning... recordsScanned=%d reviewedChangesToRemove=%d filesToDelete=%d"),
					Plan.RecordsScanned,
					Plan.ChangesRemoved,
					Plan.TransactionFilePathsToDelete.Num()
						+ Plan.SessionFilePathsToDelete.Num()
						+ Plan.OldDebugBundlePathsToDelete.Num()
						+ Plan.CompletedDebugBundlePathsToDelete.Num());
				Window->UpdateCleanupNotification(TEXT("清理中"), true, false);
				UE_LOG(LogTemp, Log, TEXT("BlueprintHelper %s"), *Window->LastCleanupStatus);

				TFuture<void> CleanupTask = Async(EAsyncExecution::ThreadPool, [WeakWindow, Plan]()
				{
					const FBlueprintHelperReviewedDataCleanupResult Result =
						FBlueprintHelperReviewedDataCleanupService::ExecuteCleanupPlan(Plan);
					AsyncTask(ENamedThreads::GameThread, [WeakWindow, Result]()
					{
						if (TSharedPtr<SBlueprintHelperMainWindow> Window = WeakWindow.Pin())
						{
							Window->bCleanupInProgress = false;
							Window->LastCleanupStatus = FString::Printf(
								TEXT("CleanReviewData reviewedChangesRemoved=%d recordsSaved=%d recordsDeleted=%d transactionFilesDeleted=%d sessionFilesDeleted=%d oldDebugBundlesDeleted=%d completedDebugBundlesDeleted=%d failures=%d error=\"%s\""),
								Result.ChangesRemoved,
								Result.RecordsSaved,
								Result.RecordsDeleted,
								Result.TransactionFilesDeleted,
								Result.SessionFilesDeleted,
								Result.OldDebugBundlesDeleted,
								Result.CompletedDebugBundlesDeleted,
								Result.Failures,
								*Result.Error);
							Window->UpdateCleanupNotification(
								Result.Failures == 0 ? TEXT("完成") : TEXT("失败"),
								Result.Failures == 0,
								true);
							UE_LOG(LogTemp, Log, TEXT("BlueprintHelper %s"), *Window->LastCleanupStatus);
							if (Window->ReviewStoreService)
							{
								Window->ReviewStoreService->NotifyPendingReviewChanged();
							}
						}
					});
				});
				TrackBlueprintHelperReviewedDataCleanupTask(MoveTemp(CleanupTask));
			}
		});
	});
	TrackBlueprintHelperReviewedDataCleanupTask(MoveTemp(ScanTask));
	return FReply::Handled();
}

void SBlueprintHelperMainWindow::ShowCleanupNotification(const FString& StatusText)
{
	FNotificationInfo Info(FText::FromString(StatusText));
	Info.bFireAndForget = false;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = false;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 4.0f;

	CleanupNotification = FSlateNotificationManager::Get().AddNotification(Info);
	if (TSharedPtr<SNotificationItem> Notification = CleanupNotification.Pin())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void SBlueprintHelperMainWindow::UpdateCleanupNotification(
	const FString& StatusText,
	bool bSucceeded,
	bool bExpire)
{
	TSharedPtr<SNotificationItem> Notification = CleanupNotification.Pin();
	if (!Notification.IsValid() && !bExpire)
	{
		ShowCleanupNotification(StatusText);
		Notification = CleanupNotification.Pin();
	}
	if (!Notification.IsValid())
	{
		return;
	}

	Notification->SetText(FText::FromString(StatusText));
	Notification->SetCompletionState(bExpire
		? (bSucceeded ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail)
		: SNotificationItem::CS_Pending);
	if (bExpire)
	{
		Notification->ExpireAndFadeout();
		CleanupNotification.Reset();
	}
}

FSlateColor SBlueprintHelperMainWindow::GetToolsTabColor() const
{
	return FSlateColor(ActivePageIndex == 0
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

FSlateColor SBlueprintHelperMainWindow::GetReviewTabColor() const
{
	return FSlateColor(ActivePageIndex == 1
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

