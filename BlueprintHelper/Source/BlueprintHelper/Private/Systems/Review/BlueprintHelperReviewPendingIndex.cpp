// BlueprintHelper Review pending index DTO implementation.

#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

FBlueprintHelperReviewStoreChangedEvent FBlueprintHelperReviewStoreChangedEvent::FullReload()
{
	FBlueprintHelperReviewStoreChangedEvent Event;
	Event.bRequiresFullReload = true;
	return Event;
}

FBlueprintHelperReviewStoreChangedEvent FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
	const TArray<FString>& InReviewRecordIds,
	const TArray<FString>& InChangeIds,
	const TArray<FString>& InAssetPaths)
{
	FBlueprintHelperReviewStoreChangedEvent Event;
	Event.ReviewRecordIds = InReviewRecordIds;
	Event.ChangeIds = InChangeIds;
	Event.AssetPaths = InAssetPaths;
	Event.bRequiresFullReload = false;
	return Event;
}
