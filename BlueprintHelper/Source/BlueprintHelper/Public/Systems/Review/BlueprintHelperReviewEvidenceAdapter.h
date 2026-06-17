// BlueprintHelper Review evidence adapter interface.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewEvidenceBuildResult
{
	bool bSucceeded = false;
	FString Message;
	FBlueprintHelperWriteReviewEvidence Evidence;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
};

class BLUEPRINTHELPER_API IBlueprintHelperReviewEvidenceAdapter
{
public:
	virtual ~IBlueprintHelperReviewEvidenceAdapter();

	virtual FString GetTargetKind() const = 0;
	virtual FBlueprintHelperReviewEvidenceBuildResult BuildEvidence(
		const FBlueprintHelperReviewEvidenceInput& Input) const = 0;
};
