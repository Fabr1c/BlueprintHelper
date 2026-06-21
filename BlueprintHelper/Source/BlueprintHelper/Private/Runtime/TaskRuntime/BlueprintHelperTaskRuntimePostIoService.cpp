// BlueprintHelper TaskRuntime post IO service.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.h"

#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"

FBlueprintHelperTaskRuntimePostIoFlushResult FBlueprintHelperTaskRuntimePostIoService::Flush(
	const FBlueprintHelperTaskRuntimePostIoBatch& Batch,
	TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals,
	const FBlueprintHelperDebugEntryService* DebugEntryService,
	FBlueprintHelperToolResultBase* MutableResultForDebugCase,
	FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace* TimingTrace) const
{
	FBlueprintHelperTaskRuntimePostIoFlushResult Result;
	FBlueprintHelperReviewStoreService ReviewStore;

	if (Batch.ArchiveSession.IsSet())
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		FString ArchiveSessionError;
		if (!ReviewStore.SaveArchiveSession(Batch.ArchiveSession.GetValue(), ArchiveSessionError))
		{
			AddDiagnostic(
				Result,
				TEXT("review_archive_session_write_failed"),
				ArchiveSessionError,
				TEXT("review.archive_session"));
		}
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.archive_session_write"),
				StageStart);
		}
	}

	if (Batch.ReviewEvidences.Num() > 0)
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		const TArray<FBlueprintHelperReviewRecord> ReviewRecords =
			ReviewStore.BuildReviewRecordsFromEvidence(Batch.ReviewEvidences);
		if (ReviewRecords.Num() == 0)
		{
			AddDiagnostic(
				Result,
				TEXT("review_evidence_produced_zero_records"),
				FString::Printf(
					TEXT("Review evidence batch produced zero review records from %d evidence item(s)."),
					Batch.ReviewEvidences.Num()),
				TEXT("review.records"));
		}
		else
		{
			FString ReviewRecordError;
			const bool bReviewRecordsSaved =
				ReviewStore.SaveReviewRecords(ReviewRecords, ReviewRecordError);
			if (!bReviewRecordsSaved)
			{
				AddDiagnostic(
					Result,
					TEXT("review_record_write_failed"),
					ReviewRecordError,
					TEXT("review.records"));
			}
			else if (Batch.SequentialReviewSessionUpdate.IsSet())
			{
				FBlueprintHelperSequentialReviewSessionExecuteUpdate Update =
					Batch.SequentialReviewSessionUpdate.GetValue();
				for (const FBlueprintHelperReviewRecord& ReviewRecord : ReviewRecords)
				{
					if (!ReviewRecord.ReviewRecordId.IsEmpty())
					{
						Update.ReviewRecordIds.AddUnique(ReviewRecord.ReviewRecordId);
					}
				}
				FBlueprintHelperSequentialReviewSession PersistedSession;
				FString SessionError;
				if (!FBlueprintHelperSequentialReviewSessionService().RecordExecuteUpdate(
					Update,
					PersistedSession,
					SessionError))
				{
					AddDiagnostic(
						Result,
						TEXT("sequential_review_session_update_failed"),
						SessionError,
						TEXT("review.sequential_session"));
				}
			}
		}
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.review_record_write"),
				StageStart);
		}
	}
	else if (Batch.SequentialReviewSessionUpdate.IsSet())
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		FBlueprintHelperSequentialReviewSession PersistedSession;
		FString SessionError;
		if (!FBlueprintHelperSequentialReviewSessionService().RecordExecuteUpdate(
			Batch.SequentialReviewSessionUpdate.GetValue(),
			PersistedSession,
			SessionError))
		{
			AddDiagnostic(
				Result,
				TEXT("sequential_review_session_update_failed"),
				SessionError,
				TEXT("review.sequential_session"));
		}
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.sequential_review_session_update"),
				StageStart);
		}
	}

	if (Batch.TaskRunJournal.IsValid() && !Batch.TaskRunId.IsEmpty())
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		TaskRunJournals.Add(Batch.TaskRunId, Batch.TaskRunJournal);
		FString JournalStoreError;
		if (!FBlueprintHelperTaskRunJournalStoreService().SaveTaskRunJournal(
			Batch.TaskRunId,
			Batch.TaskRunJournal,
			JournalStoreError))
		{
			AddDiagnostic(
				Result,
				TEXT("task_run_journal_write_failed"),
				JournalStoreError,
				TEXT("task_run_journal"));
		}
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.task_run_journal"),
				StageStart);
		}
	}

	if (DebugEntryService && Batch.DebugEventInput.IsSet())
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		if (Batch.bAttachDebugCaseToFailure && MutableResultForDebugCase)
		{
			DebugEntryService->AttachDebugCaseToFailureBestEffort(
				*MutableResultForDebugCase,
				Batch.DebugEventInput.GetValue());
		}
		else
		{
			DebugEntryService->RecordEventBestEffort(Batch.DebugEventInput.GetValue());
		}
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.debug_entry"),
				StageStart);
		}
	}

	if (Batch.bNotifyPendingReviewChanged)
	{
		const double StageStart = TimingTrace
			? FBlueprintHelperTaskRuntimeTimingUtils::StartStage(*TimingTrace)
			: 0.0;
		ReviewStore.NotifyPendingReviewChanged();
		if (TimingTrace)
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				*TimingTrace,
				TEXT("post_io.pending_review_notify"),
				StageStart);
		}
	}

	return Result;
}

void FBlueprintHelperTaskRuntimePostIoService::AddDiagnostic(
	FBlueprintHelperTaskRuntimePostIoFlushResult& Result,
	const FString& Code,
	const FString& Message,
	const FString& Field)
{
	Result.bOk = false;
	FBlueprintHelperTaskRuntimePostIoDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.Field = Field;
	Result.Diagnostics.Add(Diagnostic);
}
