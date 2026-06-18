// BlueprintHelper Review MaterialInstance evidence adapter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewEvidenceAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewMaterialInstanceEvidenceAdapter final
	: public IBlueprintHelperReviewEvidenceAdapter
{
public:
	explicit FBlueprintHelperReviewMaterialInstanceEvidenceAdapter(const FString& InTargetKind);

	virtual FString GetTargetKind() const override;
	virtual FBlueprintHelperReviewEvidenceBuildResult BuildEvidence(
		const FBlueprintHelperReviewEvidenceInput& Input) const override;

private:
	FString TargetKind;
};
