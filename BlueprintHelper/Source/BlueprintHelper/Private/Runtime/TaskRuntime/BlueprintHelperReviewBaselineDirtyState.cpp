// BlueprintHelper Review baseline dirty state model.

#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyState.h"

const TCHAR* ToString(EBlueprintHelperReviewBaselineDirtyState State)
{
	switch (State)
	{
	case EBlueprintHelperReviewBaselineDirtyState::Clean:
		return TEXT("clean");
	case EBlueprintHelperReviewBaselineDirtyState::DirtyPreexisting:
		return TEXT("dirty_preexisting");
	case EBlueprintHelperReviewBaselineDirtyState::DirtyAfterFailedExecute:
		return TEXT("dirty_after_failed_execute");
	case EBlueprintHelperReviewBaselineDirtyState::DirtyWithOpenReview:
		return TEXT("dirty_with_open_review");
	case EBlueprintHelperReviewBaselineDirtyState::DirtyExternalUserChange:
		return TEXT("dirty_external_user_change");
	case EBlueprintHelperReviewBaselineDirtyState::UnknownDirtyOrigin:
		return TEXT("unknown_dirty_origin");
	default:
		return TEXT("unknown_dirty_origin");
	}
}

EBlueprintHelperReviewBaselineDirtyState BlueprintHelperReviewBaselineDirtyStateFromString(
	const FString& Value)
{
	if (Value.Equals(TEXT("clean"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewBaselineDirtyState::Clean;
	}
	if (Value.Equals(TEXT("dirty_preexisting"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewBaselineDirtyState::DirtyPreexisting;
	}
	if (Value.Equals(TEXT("dirty_after_failed_execute"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewBaselineDirtyState::DirtyAfterFailedExecute;
	}
	if (Value.Equals(TEXT("dirty_with_open_review"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewBaselineDirtyState::DirtyWithOpenReview;
	}
	if (Value.Equals(TEXT("dirty_external_user_change"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperReviewBaselineDirtyState::DirtyExternalUserChange;
	}
	return EBlueprintHelperReviewBaselineDirtyState::UnknownDirtyOrigin;
}

