#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewDebugText
{
public:
	static FString BuildCopyableText(const TArray<FString>& Messages);
};
