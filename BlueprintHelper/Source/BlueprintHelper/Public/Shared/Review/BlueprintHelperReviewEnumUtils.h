// BlueprintHelper Review enum parsing utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewEnumUtils
{
public:
	static EBlueprintHelperReviewChangeStatus ParseChangeStatus(const FString& Status);
	static EBlueprintHelperReviewChangeKind ParseChangeKind(const FString& ChangeKind);
	static EBlueprintHelperReviewStorageStatus ParseStorageStatus(const FString& Status);
	static EBlueprintHelperReviewSurface ParseSurface(const FString& Surface);
};
