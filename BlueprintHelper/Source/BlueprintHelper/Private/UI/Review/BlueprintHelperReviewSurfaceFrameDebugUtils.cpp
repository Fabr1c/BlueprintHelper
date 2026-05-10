// BlueprintHelper Review surface frame debug utilities.

#include "UI/Review/BlueprintHelperReviewSurfaceFrameDebugUtils.h"

TSet<FString>& FBlueprintHelperReviewSurfaceFrameDebugUtils::GetEmittedFrameDebugKeys()
{
	static TSet<FString> Keys;
	return Keys;
}

void FBlueprintHelperReviewSurfaceFrameDebugUtils::EmitDedupedFrameDebug(
	const TFunction<void(const FString&)>& AddDebugMessage,
	const FString& Message,
	EBlueprintHelperReviewSurface Surface,
	const FString& ChangeId,
	const FString& Result,
	const FString& Reason)
{
	if (!AddDebugMessage)
	{
		return;
	}

	const FString Key = FString::Printf(
		TEXT("%s|%s|%s|%s"),
		BlueprintHelperReviewSurfaceToString(Surface),
		*ChangeId,
		*Result,
		*Reason);
	if (GetEmittedFrameDebugKeys().Contains(Key))
	{
		return;
	}

	GetEmittedFrameDebugKeys().Add(Key);
	AddDebugMessage(Message);
}
