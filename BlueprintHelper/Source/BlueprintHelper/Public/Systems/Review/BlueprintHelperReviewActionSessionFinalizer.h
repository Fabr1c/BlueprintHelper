// BlueprintHelper Review action sequential session finalizer.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewActionSessionFinalizer
{
public:
	FBlueprintHelperSequentialReviewSessionCloseResult CloseReviewRecordSessions(
		const FString& ReviewRecordId,
		EBlueprintHelperSequentialReviewSessionStatus FinalStatus) const;

	static void ApplyCloseResult(
		const FBlueprintHelperSequentialReviewSessionCloseResult& CloseResult,
		FBlueprintHelperReviewActionResult& InOutActionResult);
};
