// BlueprintHelper Review panel data DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewPanelDataSnapshot
{
	TArray<FBlueprintHelperReviewVisibleChange> PendingChanges;
	FString SelectedChangeId;
	FString SelectedAssetPath;

	static FBlueprintHelperReviewPanelDataSnapshot FromSelection(
		const TArray<FBlueprintHelperReviewVisibleChange>& InPendingChanges,
		const FBlueprintHelperReviewVisibleChange& InSelectedChange);
};
