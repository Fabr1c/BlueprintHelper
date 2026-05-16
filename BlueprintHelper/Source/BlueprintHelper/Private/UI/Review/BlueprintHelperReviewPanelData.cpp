// BlueprintHelper Review panel data DTOs implementation.

#include "UI/Review/BlueprintHelperReviewPanelData.h"

FBlueprintHelperReviewPanelDataSnapshot FBlueprintHelperReviewPanelDataSnapshot::FromSelection(
	const TArray<FBlueprintHelperReviewVisibleChange>& InPendingChanges,
	const FBlueprintHelperReviewVisibleChange& InSelectedChange)
{
	FBlueprintHelperReviewPanelDataSnapshot Snapshot;
	Snapshot.PendingChanges = InPendingChanges;
	Snapshot.SelectedChangeId = InSelectedChange.ChangeId;
	Snapshot.SelectedAssetPath = InSelectedChange.AssetPath;
	return Snapshot;
}
