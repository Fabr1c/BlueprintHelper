// BlueprintHelper Review action service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

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
};
