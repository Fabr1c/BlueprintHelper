// BlueprintHelper Review panel data DTOs implementation.

#include "UI/Review/BlueprintHelperReviewPanelData.h"

bool FBlueprintHelperReviewRowBinding::IsValid() const
{
	return !AssetPath.IsEmpty()
		&& !ChangeId.IsEmpty();
}

FBlueprintHelperReviewActionIntent FBlueprintHelperReviewActionIntent::Accept(
	const FBlueprintHelperReviewRowBinding& InBinding,
	const FString& InSourceWidget)
{
	FBlueprintHelperReviewActionIntent Intent;
	Intent.Action = EBlueprintHelperReviewActionIntentKind::Accept;
	Intent.Binding = InBinding;
	Intent.SourceWidget = InSourceWidget;
	return Intent;
}

FBlueprintHelperReviewActionIntent FBlueprintHelperReviewActionIntent::Reject(
	const FBlueprintHelperReviewRowBinding& InBinding,
	const FString& InSourceWidget)
{
	FBlueprintHelperReviewActionIntent Intent;
	Intent.Action = EBlueprintHelperReviewActionIntentKind::Reject;
	Intent.Binding = InBinding;
	Intent.SourceWidget = InSourceWidget;
	return Intent;
}

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
