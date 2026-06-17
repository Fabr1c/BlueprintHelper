// BlueprintHelper ReviewPanel debug focus traversal coordinator.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

enum class EBlueprintHelperReviewDebugFocusTraversalStepKind : uint8
{
	None,
	AlreadyRunning,
	SkippedNoRows,
	Started,
	Focus,
	Complete,
	FocusReady,
	WaitGeometry
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDebugFocusTraversalStep
{
	EBlueprintHelperReviewDebugFocusTraversalStepKind Kind =
		EBlueprintHelperReviewDebugFocusTraversalStepKind::None;
	FString EventStage;
	FString Message;
	FString Reason;
	int32 EventIndex = 0;
	int32 Total = 0;
	TSharedPtr<FBlueprintHelperReviewVisibleChange> Item;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDebugFocusTraversalCoordinator
{
public:
	typedef TSharedPtr<FBlueprintHelperReviewVisibleChange> FChangeItem;
	typedef TFunction<bool(FChangeItem, FString&)> FGeometryReadyPredicate;

	FBlueprintHelperReviewDebugFocusTraversalStep Start(
		const TArray<FChangeItem>& ChangeItems,
		const FString& CurrentAssetPath,
		bool bFilterCurrentAssetOnly);

	FBlueprintHelperReviewDebugFocusTraversalStep Advance();

	FBlueprintHelperReviewDebugFocusTraversalStep ProcessGeometryEvent(
		const FGeometryReadyPredicate& IsGeometryReady);

	bool IsActive() const;
	bool IsAwaitingGeometry() const;
	int32 GetCurrentIndex() const;
	int32 GetTotal() const;

private:
	TArray<FChangeItem> TraversalItems;
	int32 TraversalIndex = 0;
	bool bTraversalAwaitingGeometry = false;
	bool bTraversalActive = false;
};
