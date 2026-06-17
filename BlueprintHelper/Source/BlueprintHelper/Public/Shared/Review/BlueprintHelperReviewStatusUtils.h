// BlueprintHelper Review status utility helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewStatusUtils
{
public:
	static bool IsOpenReviewStatus(EBlueprintHelperReviewChangeStatus Status);

	static bool ReviewTargetMatches(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TArray<FString>& TargetKeys);

	static bool ReviewTargetMatches(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSet<FString>& TargetKeys);

	static EBlueprintHelperReviewChangeStatus CombineTargetStatuses(
		const TArray<FBlueprintHelperReviewAtomicTarget>& Targets);

	static EBlueprintHelperReviewChangeStatus CombineChangeStatuses(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes);

	static void RefreshReviewRecordStatus(FBlueprintHelperReviewRecord& Record);
};
