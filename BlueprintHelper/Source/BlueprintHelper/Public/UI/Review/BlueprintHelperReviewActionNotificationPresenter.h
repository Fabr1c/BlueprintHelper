// BlueprintHelper Review action notification presenter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class SNotificationItem;

enum class EBlueprintHelperReviewActionNotificationState : uint8
{
	Pending,
	Success,
	Fail
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewActionBatchNotificationState
{
	FString NotificationKey;
	int32 TotalCount = 0;
	int32 FinishedCount = 0;
	int32 SuccessCount = 0;
	int32 FailedCount = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewActionNotificationPresenter
{
public:
	void Show(
		const FString& NotificationKey,
		const FString& StatusText,
		EBlueprintHelperReviewActionNotificationState State,
		bool bExpire,
		bool bUseThrobber);

	static FString BuildChangeLabel(const FBlueprintHelperReviewVisibleChange* Change);

	bool IsChangeInBatch(const FString& ChangeId) const;
	void RegisterBatch(const FString& BatchKey, int32 TotalCount);
	void AddChangeToBatch(const FString& ChangeId, const FString& BatchKey);
	void RecordBatchResult(const FString& ChangeId, bool bSucceeded);

private:
	TMap<FString, TWeakPtr<SNotificationItem>> NotificationsByKey;
	TMap<FString, FString> BatchKeyByChangeId;
	TMap<FString, FBlueprintHelperReviewActionBatchNotificationState> BatchesByKey;
};
