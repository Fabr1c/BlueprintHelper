// BlueprintHelper Review target validity resolver.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewTargetValidityResolver
{
public:
	FBlueprintHelperReviewValidityResult ValidateOnGameThread(
		const FBlueprintHelperReviewValidityCandidate& Candidate) const;
};
