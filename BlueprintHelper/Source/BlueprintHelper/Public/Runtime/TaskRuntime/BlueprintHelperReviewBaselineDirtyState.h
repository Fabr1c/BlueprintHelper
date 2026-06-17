// BlueprintHelper Review baseline dirty state model.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperReviewBaselineDirtyState : uint8
{
	Clean,
	DirtyPreexisting,
	DirtyAfterFailedExecute,
	DirtyWithOpenReview,
	DirtyExternalUserChange,
	UnknownDirtyOrigin
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineDirtyDecision
{
	EBlueprintHelperReviewBaselineDirtyState State = EBlueprintHelperReviewBaselineDirtyState::Clean;
	FString Code;
	FString Category;
	FString Stage;
	FString SafeNextAction;
	TArray<FString> DirtyAssets;
	TArray<FString> AllowedRecoveryActions;
	TArray<FString> RiskyRecoveryActions;
	TArray<FString> EvidenceRefs;

	bool IsDirty() const
	{
		return State != EBlueprintHelperReviewBaselineDirtyState::Clean;
	}
};

BLUEPRINTHELPER_API const TCHAR* ToString(EBlueprintHelperReviewBaselineDirtyState State);
BLUEPRINTHELPER_API EBlueprintHelperReviewBaselineDirtyState BlueprintHelperReviewBaselineDirtyStateFromString(
	const FString& Value);

