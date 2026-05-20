// BlueprintHelper Review FBlueprintHelperReviewStoreMergeUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

class FBlueprintHelperReviewStoreMergeUtils
{
public:
	static FString MakeVisibleChangeScopeIdentity(const FBlueprintHelperReviewVisibleChange& Change);
	static FString MakeLoadedVisibleChangeChangeIdCollapseKey(const FBlueprintHelperReviewVisibleChange& Change);
	static FString MakeLoadedVisibleChangeLifecycleRootCollapseKey(const FBlueprintHelperReviewVisibleChange& Change);
	static void AddUniqueReviewStrings(TArray<FString>& Target, const TArray<FString>& Source);
	static void MergeReviewAtomicTargetsLatestWins(
				TArray<FBlueprintHelperReviewAtomicTarget>& ExistingTargets,
				const TArray<FBlueprintHelperReviewAtomicTarget>& IncomingTargets);
	static void MergeVisibleChangeLatestWins(
				FBlueprintHelperReviewVisibleChange& Existing,
				const FBlueprintHelperReviewVisibleChange& Incoming);
	static void RemoveNetNoChangeVisibleChanges(TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	static void CollapseVisibleChangesLatestWins(TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	static void MergeReviewRecord(FBlueprintHelperReviewRecord& Existing, const FBlueprintHelperReviewRecord& Incoming);
};
