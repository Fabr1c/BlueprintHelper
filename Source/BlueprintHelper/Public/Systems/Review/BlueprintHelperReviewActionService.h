// BlueprintHelper Review action service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperDebugEntryService;

struct FBlueprintHelperReviewActionResult
{
	bool bSucceeded = false;
	FString TargetTransactionId;
	EBlueprintHelperReviewChangeStatus NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	FString RollbackMode;
	FString Message;
	bool bSupersededDataCompactionEligible = false;
};

struct FBlueprintHelperReviewRejectOptions
{
	TMap<FString, FString> CurrentHashesByTargetKey;
	bool bRollbackExecutorAvailable = false;
	bool bRollbackSucceeded = false;
	FString RollbackFailureMessage;
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

	FBlueprintHelperReviewActionResult AcceptReviewTargets(
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys) const;

	FBlueprintHelperReviewActionResult RejectReviewTargets(
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewActionResult RejectAll(
		const FBlueprintHelperReviewRecordQuery& Query,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewActionResult ConvertOwnerBlock(
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request) const;

private:
	void RecordRejectDebugCaseBestEffort(
		FBlueprintHelperReviewRecord& Record,
		const TArray<FString>& TargetKeys,
		const FString& SourceTransactionId,
		EBlueprintHelperReviewChangeStatus RejectStatus,
		const FString& RejectMessage) const;

	const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr;
};
