// BlueprintHelper Review panel v2 state service.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPanelStateService
{
public:
	static FBlueprintHelperReviewPanelState BuildPanelState(
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange);

	static FBlueprintHelperReviewRowBinding MakeChangeBinding(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetKey = FString());

	static FBlueprintHelperReviewRowBinding MakeTargetBinding(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewAtomicTarget& Target,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetKey = FString());

	static bool TryFindChangeByIntent(
		const FBlueprintHelperReviewActionIntent& Intent,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		FBlueprintHelperReviewVisibleChange& OutChange);

	static void SetTransientActionState(
		FBlueprintHelperReviewPanelState& State,
		const FString& ChangeId,
		EBlueprintHelperReviewActionIntentKind Action,
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message);

	static void ClearTransientActionState(
		FBlueprintHelperReviewPanelState& State,
		const FString& ChangeId);

	static bool IsTransientActionInProgress(
		const FBlueprintHelperReviewPanelState& State,
		const FString& ChangeId);

	static void SetPresenterErrorState(
		FBlueprintHelperReviewPanelState& State,
		const FString& ChangeId,
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message);

	static void ClearPresenterErrorState(
		FBlueprintHelperReviewPanelState& State,
		const FString& ChangeId);

	static void CollectTargetKeysForSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		TArray<FString>& OutKeys);

private:
	static void AddTargetKey(const FString& Key, TArray<FString>& OutKeys);
	static FString MakeAtomicTargetId(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewAtomicTarget* Target,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetKey);
};
