// BlueprintHelper Review baseline dirty debug evidence projection.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineDirtyDebugEvidenceProjection
{
public:
	static FString ClassifyEvidenceRefRole(const FString& EvidenceRef);

	static TArray<FBlueprintHelperDebugEvidenceLink> MakeEvidenceLinksFromRefs(
		const TArray<FString>& EvidenceRefs);
};
