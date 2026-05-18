// BlueprintHelper Review status utility helpers implementation.

#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"

bool FBlueprintHelperReviewStatusUtils::ReviewTargetMatches(
	const FBlueprintHelperReviewAtomicTarget& Target,
	const TArray<FString>& TargetKeys)
{
	return TargetKeys.Num() == 0 || TargetKeys.Contains(Target.TargetKey);
}

bool FBlueprintHelperReviewStatusUtils::ReviewTargetMatches(
	const FBlueprintHelperReviewAtomicTarget& Target,
	const TSet<FString>& TargetKeys)
{
	return TargetKeys.Num() == 0 || TargetKeys.Contains(Target.TargetKey);
}

EBlueprintHelperReviewChangeStatus FBlueprintHelperReviewStatusUtils::CombineTargetStatuses(
	const TArray<FBlueprintHelperReviewAtomicTarget>& Targets)
{
	if (Targets.Num() == 0)
	{
		return EBlueprintHelperReviewChangeStatus::NeedsAction;
	}

	bool bAllAccepted = true;
	bool bAllRejected = true;
	bool bAnyPending = false;
	bool bAnyNeedsAction = false;
	bool bAnyRejectFailed = false;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Targets)
	{
		bAllAccepted &= Target.Status == EBlueprintHelperReviewChangeStatus::Accepted;
		bAllRejected &= Target.Status == EBlueprintHelperReviewChangeStatus::Rejected;
		bAnyPending |= Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
		bAnyNeedsAction |= Target.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
		bAnyRejectFailed |= Target.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	if (bAnyRejectFailed)
	{
		return EBlueprintHelperReviewChangeStatus::RejectFailed;
	}
	if (bAnyNeedsAction)
	{
		return EBlueprintHelperReviewChangeStatus::NeedsAction;
	}
	if (bAnyPending)
	{
		return EBlueprintHelperReviewChangeStatus::Pending;
	}
	if (bAllAccepted)
	{
		return EBlueprintHelperReviewChangeStatus::Accepted;
	}
	if (bAllRejected)
	{
		return EBlueprintHelperReviewChangeStatus::Rejected;
	}
	return EBlueprintHelperReviewChangeStatus::Pending;
}

EBlueprintHelperReviewChangeStatus FBlueprintHelperReviewStatusUtils::CombineChangeStatuses(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
{
	if (Changes.Num() == 0)
	{
		return EBlueprintHelperReviewChangeStatus::NeedsAction;
	}

	bool bAllAccepted = true;
	bool bAllRejected = true;
	bool bAnyPending = false;
	bool bAnyNeedsAction = false;
	bool bAnyRejectFailed = false;
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		bAllAccepted &= Change.Status == EBlueprintHelperReviewChangeStatus::Accepted;
		bAllRejected &= Change.Status == EBlueprintHelperReviewChangeStatus::Rejected;
		bAnyPending |= Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
		bAnyNeedsAction |= Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
		bAnyRejectFailed |= Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	if (bAnyRejectFailed)
	{
		return EBlueprintHelperReviewChangeStatus::RejectFailed;
	}
	if (bAnyNeedsAction)
	{
		return EBlueprintHelperReviewChangeStatus::NeedsAction;
	}
	if (bAnyPending)
	{
		return EBlueprintHelperReviewChangeStatus::Pending;
	}
	if (bAllAccepted)
	{
		return EBlueprintHelperReviewChangeStatus::Accepted;
	}
	if (bAllRejected)
	{
		return EBlueprintHelperReviewChangeStatus::Rejected;
	}
	return EBlueprintHelperReviewChangeStatus::Pending;
}

void FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(FBlueprintHelperReviewRecord& Record)
{
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		Change.Status = CombineTargetStatuses(Change.AtomicTargets);
	}
	Record.Status = CombineChangeStatuses(Record.VisibleChanges);
	Record.SourceReviewSummary.FinalReviewStatus = Record.Status;
}
