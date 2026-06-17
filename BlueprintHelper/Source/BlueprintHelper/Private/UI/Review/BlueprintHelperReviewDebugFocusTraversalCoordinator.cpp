// BlueprintHelper ReviewPanel debug focus traversal coordinator implementation.

#include "UI/Review/BlueprintHelperReviewDebugFocusTraversalCoordinator.h"

FBlueprintHelperReviewDebugFocusTraversalStep FBlueprintHelperReviewDebugFocusTraversalCoordinator::Start(
	const TArray<FChangeItem>& ChangeItems,
	const FString& CurrentAssetPath,
	bool bFilterCurrentAssetOnly)
{
	FBlueprintHelperReviewDebugFocusTraversalStep Step;
	if (bTraversalActive)
	{
		Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::AlreadyRunning;
		Step.Message = TEXT("Debug focus traversal is already running.");
		return Step;
	}

	TraversalItems.Reset();
	for (const FChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (bFilterCurrentAssetOnly && !CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}
		TraversalItems.Add(Item);
	}
	if (TraversalItems.Num() == 0)
	{
		for (const FChangeItem& Item : ChangeItems)
		{
			if (Item.IsValid())
			{
				TraversalItems.Add(Item);
			}
		}
	}
	if (TraversalItems.Num() == 0)
	{
		Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::SkippedNoRows;
		Step.Message = TEXT("Debug focus traversal skipped: no final change rows available.");
		return Step;
	}

	TraversalIndex = 0;
	bTraversalAwaitingGeometry = false;
	bTraversalActive = true;

	Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::Started;
	Step.EventStage = TEXT("start");
	Step.EventIndex = 0;
	Step.Total = TraversalItems.Num();
	Step.Message = FString::Printf(
		TEXT("Debug focus traversal started: %d rows."),
		TraversalItems.Num());
	return Step;
}

FBlueprintHelperReviewDebugFocusTraversalStep FBlueprintHelperReviewDebugFocusTraversalCoordinator::Advance()
{
	FBlueprintHelperReviewDebugFocusTraversalStep Step;
	if (!bTraversalActive)
	{
		return Step;
	}

	if (!TraversalItems.IsValidIndex(TraversalIndex))
	{
		bTraversalActive = false;
		bTraversalAwaitingGeometry = false;
		Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::Complete;
		Step.EventStage = TEXT("complete");
		Step.EventIndex = TraversalItems.Num();
		Step.Total = TraversalItems.Num();
		Step.Message = FString::Printf(
			TEXT("Debug focus traversal completed: %d rows."),
			TraversalItems.Num());
		return Step;
	}

	Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::Focus;
	Step.EventStage = TEXT("focus");
	Step.EventIndex = TraversalIndex + 1;
	Step.Total = TraversalItems.Num();
	Step.Item = TraversalItems[TraversalIndex];
	bTraversalAwaitingGeometry = true;
	return Step;
}

FBlueprintHelperReviewDebugFocusTraversalStep
FBlueprintHelperReviewDebugFocusTraversalCoordinator::ProcessGeometryEvent(
	const FGeometryReadyPredicate& IsGeometryReady)
{
	FBlueprintHelperReviewDebugFocusTraversalStep Step;
	if (!bTraversalActive || !bTraversalAwaitingGeometry)
	{
		return Step;
	}

	const FChangeItem Item = TraversalItems.IsValidIndex(TraversalIndex)
		? TraversalItems[TraversalIndex]
		: FChangeItem();
	FString GeometryReason;
	const bool bReady = IsGeometryReady
		? IsGeometryReady(Item, GeometryReason)
		: true;

	Step.EventIndex = TraversalIndex + 1;
	Step.Total = TraversalItems.Num();
	Step.Item = Item;
	Step.Reason = GeometryReason;
	if (bReady)
	{
		bTraversalAwaitingGeometry = false;
		++TraversalIndex;
		Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::FocusReady;
		Step.EventStage = TEXT("focus_ready");
		return Step;
	}

	Step.Kind = EBlueprintHelperReviewDebugFocusTraversalStepKind::WaitGeometry;
	Step.EventStage = TEXT("wait_geometry_event");
	return Step;
}

bool FBlueprintHelperReviewDebugFocusTraversalCoordinator::IsActive() const
{
	return bTraversalActive;
}

bool FBlueprintHelperReviewDebugFocusTraversalCoordinator::IsAwaitingGeometry() const
{
	return bTraversalAwaitingGeometry;
}

int32 FBlueprintHelperReviewDebugFocusTraversalCoordinator::GetCurrentIndex() const
{
	return TraversalIndex;
}

int32 FBlueprintHelperReviewDebugFocusTraversalCoordinator::GetTotal() const
{
	return TraversalItems.Num();
}
