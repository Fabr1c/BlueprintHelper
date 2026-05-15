// BlueprintHelper main window shell implementation.

#include "UI/SBlueprintHelperMainWindow.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/SHelperMainWidget.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FBlueprintHelperReviewedDataCleanupResult
{
	int32 RecordsSaved = 0;
	int32 RecordsDeleted = 0;
	int32 ChangesRemoved = 0;
	int32 TransactionFilesDeleted = 0;
	int32 SessionFilesDeleted = 0;
	int32 OldDebugBundlesDeleted = 0;
	int32 CompletedDebugBundlesDeleted = 0;
	int32 Failures = 0;
	FString Error;
};

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

static bool IsBlueprintHelperReviewedTerminalStatus(EBlueprintHelperReviewChangeStatus Status)
{
	return Status == EBlueprintHelperReviewChangeStatus::Accepted
		|| Status == EBlueprintHelperReviewChangeStatus::Rejected
		|| Status == EBlueprintHelperReviewChangeStatus::Superseded;
}

static bool IsBlueprintHelperOpenReviewStatusText(const FString& Status)
{
	return Status.Equals(TEXT("pending"), ESearchCase::IgnoreCase)
		|| Status.Equals(TEXT("needs_action"), ESearchCase::IgnoreCase)
		|| Status.Equals(TEXT("reject_failed"), ESearchCase::IgnoreCase);
}

static void AddBlueprintHelperReviewTransactionId(TSet<FString>& TransactionIds, const FString& TransactionId)
{
	if (!TransactionId.IsEmpty())
	{
		TransactionIds.Add(TransactionId);
	}
}

static void AddBlueprintHelperRollbackRefTransactionId(TSet<FString>& TransactionIds, const FString& RollbackDataRef)
{
	const FString Prefix = TEXT("transaction://");
	const FString Suffix = TEXT("/rollback_data");
	if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
	{
		return;
	}
	FString TransactionId = RollbackDataRef.Mid(Prefix.Len());
	TransactionId.LeftChopInline(Suffix.Len());
	AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
}

static void CollectBlueprintHelperRetainedTransactionIds(
	const FBlueprintHelperReviewVisibleChange& Change,
	TSet<FString>& TransactionIds)
{
	AddBlueprintHelperReviewTransactionId(TransactionIds, Change.LatestTransactionId);
	for (const FString& TransactionId : Change.LatestTransactionIds)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FString& TransactionId : Change.SourceTransactionIds)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, Target.FirstTransactionId);
		AddBlueprintHelperReviewTransactionId(TransactionIds, Target.LatestTransactionId);
		for (const FString& TransactionId : Target.SourceTransactionIds)
		{
			AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
		}
		AddBlueprintHelperRollbackRefTransactionId(TransactionIds, Target.RollbackDataRef);
	}
}

static void InspectBlueprintHelperDebugBundleChangeObject(
	const TSharedPtr<FJsonObject>& ChangeObject,
	bool& bHasReviewStatus,
	bool& bHasOpenReviewStatus)
{
	if (!ChangeObject.IsValid())
	{
		return;
	}

	bool bValidChange = true;
	ChangeObject->TryGetBoolField(TEXT("valid"), bValidChange);
	if (!bValidChange)
	{
		return;
	}

	FString Status;
	if (ChangeObject->TryGetStringField(TEXT("status"), Status) && !Status.IsEmpty())
	{
		bHasReviewStatus = true;
		if (IsBlueprintHelperOpenReviewStatusText(Status))
		{
			bHasOpenReviewStatus = true;
		}
	}
}

static bool IsBlueprintHelperCompletedReviewPanelBundle(const FString& BundlePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *BundlePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Bundle;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Bundle) || !Bundle.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (!Bundle->TryGetArrayField(TEXT("events"), Events) || !Events)
	{
		return false;
	}

	bool bHasReviewStatus = false;
	bool bHasOpenReviewStatus = false;
	for (const TSharedPtr<FJsonValue>& EventValue : *Events)
	{
		const TSharedPtr<FJsonObject> EventObject = EventValue.IsValid()
			? EventValue->AsObject()
			: nullptr;
		if (!EventObject.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* SelectedChangeObject = nullptr;
		if (EventObject->TryGetObjectField(TEXT("selected_change"), SelectedChangeObject)
			&& SelectedChangeObject)
		{
			InspectBlueprintHelperDebugBundleChangeObject(
				*SelectedChangeObject,
				bHasReviewStatus,
				bHasOpenReviewStatus);
		}

		const TSharedPtr<FJsonObject>* ChangeObject = nullptr;
		if (EventObject->TryGetObjectField(TEXT("change"), ChangeObject)
			&& ChangeObject)
		{
			InspectBlueprintHelperDebugBundleChangeObject(
				*ChangeObject,
				bHasReviewStatus,
				bHasOpenReviewStatus);
		}
	}

	return bHasReviewStatus && !bHasOpenReviewStatus;
}

static void CleanupBlueprintHelperReviewPanelBundles(
	int32& OldDeletedCount,
	int32& CompletedDeletedCount,
	int32& FailureCount)
{
	const FString BundleDir = FBlueprintHelperReviewDebugBundleService::GetReviewPanelBundleDir();
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*BundleDir))
	{
		return;
	}

	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *(BundleDir / TEXT("*.json")), true, false);
	const FDateTime OldBundleCutoff = FDateTime::Now() - FTimespan::FromDays(7);
	for (const FString& FileName : FileNames)
	{
		const FString Path = BundleDir / FileName;
		const FDateTime Timestamp = FileManager.GetTimeStamp(*Path);
		const bool bOldBundle = Timestamp != FDateTime::MinValue()
			&& Timestamp < OldBundleCutoff;
		const bool bCompletedSessionBundle = IsBlueprintHelperCompletedReviewPanelBundle(Path);
		if (!bOldBundle && !bCompletedSessionBundle)
		{
			continue;
		}

		if (FileManager.Delete(*Path, false, true))
		{
			if (bOldBundle)
			{
				++OldDeletedCount;
			}
			else
			{
				++CompletedDeletedCount;
			}
		}
		else
		{
			++FailureCount;
		}
	}
}

static void DeleteBlueprintHelperUnreferencedJsonFiles(
	const FString& Directory,
	const TSet<FString>& RetainedIds,
	int32& DeletedCount,
	int32& FailureCount)
{
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*Directory))
	{
		return;
	}

	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *(Directory / TEXT("*.json")), true, false);
	for (const FString& FileName : FileNames)
	{
		const FString Id = FPaths::GetBaseFilename(FileName);
		if (RetainedIds.Contains(Id))
		{
			continue;
		}
		const FString Path = Directory / FileName;
		if (FileManager.Delete(*Path, false, true))
		{
			++DeletedCount;
		}
		else
		{
			++FailureCount;
		}
	}
}

static FBlueprintHelperReviewedDataCleanupResult CleanupBlueprintHelperReviewedData()
{
	FBlueprintHelperReviewedDataCleanupResult Result;
	SBlueprintHelperReviewPanel::FlushAsyncTasks();

	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecordQuery Query;
	Query.bPendingOnly = false;
	TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	TSet<FString> RetainedTransactionIds;
	TSet<FString> RetainedArchiveSessionIds;

	for (FBlueprintHelperReviewRecord& Record : Records)
	{
		TArray<FBlueprintHelperReviewVisibleChange> RetainedChanges;
		bool bRemovedTerminalChange = false;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			if (IsBlueprintHelperReviewedTerminalStatus(Change.Status))
			{
				++Result.ChangesRemoved;
				bRemovedTerminalChange = true;
				continue;
			}
			RetainedChanges.Add(Change);
			CollectBlueprintHelperRetainedTransactionIds(Change, RetainedTransactionIds);
		}

		if (RetainedChanges.Num() > 0)
		{
			RetainedArchiveSessionIds.Add(Record.ArchiveSessionId);
			if (bRemovedTerminalChange)
			{
				Record.VisibleChanges = MoveTemp(RetainedChanges);
				FString SaveError;
				if (Store.SaveReviewRecord(Record, SaveError))
				{
					++Result.RecordsSaved;
				}
				else
				{
					++Result.Failures;
					Result.Error = SaveError;
				}
			}
			continue;
		}

		FString DeleteError;
		if (Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError))
		{
			++Result.RecordsDeleted;
		}
		else
		{
			++Result.Failures;
			Result.Error = DeleteError;
		}
	}

	const FString BlueprintHelperSavedRoot = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper");
	DeleteBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Transactions") / TEXT("Active"),
		RetainedTransactionIds,
		Result.TransactionFilesDeleted,
		Result.Failures);
	DeleteBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review"),
		RetainedTransactionIds,
		Result.TransactionFilesDeleted,
		Result.Failures);
	DeleteBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review") / TEXT("Sessions"),
		RetainedArchiveSessionIds,
		Result.SessionFilesDeleted,
		Result.Failures);
	CleanupBlueprintHelperReviewPanelBundles(
		Result.OldDebugBundlesDeleted,
		Result.CompletedDebugBundlesDeleted,
		Result.Failures);

	return Result;
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
	LastCleanupStatus = TEXT("CleanReviewData running...");
	TWeakPtr<SBlueprintHelperMainWindow> WeakWindow =
		StaticCastSharedRef<SBlueprintHelperMainWindow>(AsShared());
	TFuture<void> CleanupTask = Async(EAsyncExecution::ThreadPool, [WeakWindow]()
	{
		const FBlueprintHelperReviewedDataCleanupResult Result =
			CleanupBlueprintHelperReviewedData();
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
				UE_LOG(LogTemp, Log, TEXT("BlueprintHelper %s"), *Window->LastCleanupStatus);
				if (Window->ReviewStoreService)
				{
					Window->ReviewStoreService->NotifyPendingReviewChanged();
				}
			}
		});
	});
	TrackBlueprintHelperReviewedDataCleanupTask(MoveTemp(CleanupTask));
	return FReply::Handled();
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
