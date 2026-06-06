#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewBoundaryModel.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewTargetMatcher
{
public:
	static bool MatchesTargetKey(
		const FBlueprintHelperReviewBoundaryModel& Boundary,
		const FString& TargetKey);
	static bool MatchesAnyTargetKey(
		const FBlueprintHelperReviewBoundaryModel& Boundary,
		const TArray<FString>& TargetKeys);
	static bool MatchesScopeIdentity(
		const FBlueprintHelperReviewBoundaryModel& Boundary,
		const FString& ScopeIdentity);
	static bool BelongsToLifecycleRoot(
		const FBlueprintHelperReviewBoundaryModel& Child,
		const FBlueprintHelperReviewBoundaryModel& Root);
};
