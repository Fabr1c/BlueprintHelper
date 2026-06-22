// BlueprintHelper Review action service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperDebugEntryService;
class FBlueprintHelperReviewAdapterRegistry;

struct FBlueprintHelperReviewActionResult
{
	bool bSucceeded = false;
	FString TargetEvidenceId;
	EBlueprintHelperReviewChangeStatus NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	FString RollbackMode;
	FString Message;
	bool bSupersededDataCompactionEligible = false;
	bool bReviewActionCommitted = false;
	bool bSessionCloseCommitted = true;
	FString HashGuardTargetKey;
	FString HashGuardExpectedHash;
	FString HashGuardCurrentHash;
	FString HashGuardCurrentSnapshotJson;
	FString HashGuardRecordedAfterSnapshotJson;
	FString SessionCloseErrorCode;
	FString SessionCloseErrorMessage;
	FString SafeNextAction;
	TArray<FString> AffectedSequentialReviewSessionIds;
};

struct FBlueprintHelperReviewCascadeActionResult
{
	FBlueprintHelperReviewActionResult RootResult;
	TArray<FString> RemovedChildChangeIds;
	bool bChildrenRemoved = false;
};

struct FBlueprintHelperReviewBatchActionResult
{
	bool bSucceeded = false;
	int32 RequestedCount = 0;
	int32 SucceededCount = 0;
	int32 FailedCount = 0;
	TArray<FBlueprintHelperReviewActionResult> Results;
	TArray<FString> ChangedReviewRecordIds;
	FString Message;
};

struct FBlueprintHelperReviewRejectOptions
{
	TMap<FString, FString> CurrentHashesByTargetKey;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewActionService
{
public:
	FBlueprintHelperReviewActionService();
	explicit FBlueprintHelperReviewActionService(const FBlueprintHelperDebugEntryService* InDebugEntryService);

	FBlueprintHelperReviewActionResult AcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;

	FBlueprintHelperReviewActionResult RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;

	FBlueprintHelperReviewActionResult RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges) const;

	FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Root,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewActionResult AcceptReviewTargets(
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys) const;

	FBlueprintHelperReviewActionResult RejectReviewTargets(
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewBatchActionResult AcceptReviewTargetsBatch(
		const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId) const;

	FBlueprintHelperReviewBatchActionResult RejectReviewTargetsBatch(
		const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewActionResult RejectAll(
		const FBlueprintHelperReviewRecordQuery& Query,
		const FBlueprintHelperReviewRejectOptions& Options) const;

private:
	FBlueprintHelperReviewActionResult RejectResolvedVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	void RecordRejectDebugCaseBestEffort(
		FBlueprintHelperReviewRecord& Record,
		const TArray<FString>& TargetKeys,
		const FString& SourceEvidenceId,
		EBlueprintHelperReviewChangeStatus RejectStatus,
		const FString& RejectMessage) const;

	const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr;
	TSharedPtr<FBlueprintHelperReviewAdapterRegistry> AdapterRegistry;
};
