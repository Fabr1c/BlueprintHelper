// BlueprintHelper Review action notification presenter implementation.

#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void FBlueprintHelperReviewActionNotificationPresenter::Show(
	const FString& NotificationKey,
	const FString& StatusText,
	EBlueprintHelperReviewActionNotificationState State,
	bool bExpire,
	bool bUseThrobber)
{
	const FString EffectiveKey = NotificationKey.IsEmpty() ? TEXT("review_action") : NotificationKey;
	TSharedPtr<SNotificationItem> Notification;
	if (TWeakPtr<SNotificationItem>* ExistingNotification = NotificationsByKey.Find(EffectiveKey))
	{
		Notification = ExistingNotification->Pin();
	}

	if (!Notification.IsValid())
	{
		FNotificationInfo Info(FText::FromString(StatusText));
		Info.bFireAndForget = false;
		Info.bUseThrobber = bUseThrobber;
		Info.bUseSuccessFailIcons = true;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = 3.0f;
		Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			NotificationsByKey.Add(EffectiveKey, Notification);
		}
	}

	if (!Notification.IsValid())
	{
		return;
	}

	Notification->SetText(FText::FromString(StatusText));
	switch (State)
	{
	case EBlueprintHelperReviewActionNotificationState::Pending:
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		break;
	case EBlueprintHelperReviewActionNotificationState::Success:
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		break;
	case EBlueprintHelperReviewActionNotificationState::Fail:
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
		break;
	default:
		Notification->SetCompletionState(SNotificationItem::CS_None);
		break;
	}

	if (bExpire)
	{
		Notification->ExpireAndFadeout();
		NotificationsByKey.Remove(EffectiveKey);
	}
}

FString FBlueprintHelperReviewActionNotificationPresenter::BuildChangeLabel(
	const FBlueprintHelperReviewVisibleChange* Change)
{
	if (!Change)
	{
		return TEXT("unknown change");
	}
	if (!Change->DisplayLabel.IsEmpty())
	{
		return Change->DisplayLabel;
	}
	if (!Change->ChangeId.IsEmpty())
	{
		return Change->ChangeId;
	}
	return Change->LatestEvidenceId.IsEmpty() ? TEXT("unknown change") : Change->LatestEvidenceId;
}

bool FBlueprintHelperReviewActionNotificationPresenter::IsChangeInBatch(const FString& ChangeId) const
{
	return BatchKeyByChangeId.Contains(ChangeId);
}

void FBlueprintHelperReviewActionNotificationPresenter::RegisterBatch(
	const FString& BatchKey,
	int32 TotalCount)
{
	if (BatchKey.IsEmpty())
	{
		return;
	}

	FBlueprintHelperReviewActionBatchNotificationState& Batch = BatchesByKey.FindOrAdd(BatchKey);
	Batch.NotificationKey = BatchKey;
	Batch.TotalCount = TotalCount;
}

void FBlueprintHelperReviewActionNotificationPresenter::AddChangeToBatch(
	const FString& ChangeId,
	const FString& BatchKey)
{
	if (ChangeId.IsEmpty() || BatchKey.IsEmpty())
	{
		return;
	}
	BatchKeyByChangeId.Add(ChangeId, BatchKey);
}

void FBlueprintHelperReviewActionNotificationPresenter::RecordBatchResult(
	const FString& ChangeId,
	bool bSucceeded)
{
	const FString* BatchKeyPtr = BatchKeyByChangeId.Find(ChangeId);
	if (!BatchKeyPtr)
	{
		return;
	}

	const FString BatchKey = *BatchKeyPtr;
	BatchKeyByChangeId.Remove(ChangeId);
	FBlueprintHelperReviewActionBatchNotificationState* Batch = BatchesByKey.Find(BatchKey);
	if (!Batch)
	{
		return;
	}

	++Batch->FinishedCount;
	if (bSucceeded)
	{
		++Batch->SuccessCount;
	}
	else
	{
		++Batch->FailedCount;
	}

	if (Batch->FinishedCount < Batch->TotalCount)
	{
		Show(
			Batch->NotificationKey,
			FString::Printf(
				TEXT("Reject all running: %d/%d item(s)."),
				Batch->FinishedCount,
				Batch->TotalCount),
			EBlueprintHelperReviewActionNotificationState::Pending,
			false,
			true);
		return;
	}

	const FString FinalText = Batch->FailedCount == 0
		? FString::Printf(TEXT("Reject all succeeded: %d item(s)."), Batch->SuccessCount)
		: Batch->SuccessCount > 0
			? FString::Printf(TEXT("Reject all partially succeeded: %d rejected, %d failed."), Batch->SuccessCount, Batch->FailedCount)
			: FString::Printf(TEXT("Reject all failed: %d item(s) failed."), Batch->FailedCount);
	Show(
		Batch->NotificationKey,
		FinalText,
		Batch->FailedCount == 0
			? EBlueprintHelperReviewActionNotificationState::Success
			: EBlueprintHelperReviewActionNotificationState::Fail,
		true,
		false);
	BatchesByKey.Remove(BatchKey);
}
