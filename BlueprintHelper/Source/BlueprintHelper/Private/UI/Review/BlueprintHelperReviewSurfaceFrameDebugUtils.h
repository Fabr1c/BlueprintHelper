// BlueprintHelper Review surface frame debug utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperReviewSurfaceFrameDebugUtils
{
public:
	static void EmitDedupedFrameDebug(
		const TFunction<void(const FString&)>& AddDebugMessage,
		const FString& Message,
		EBlueprintHelperReviewSurface Surface,
		const FString& ChangeId,
		const FString& Result,
		const FString& Reason);

private:
	static TSet<FString>& GetEmittedFrameDebugKeys();
};
