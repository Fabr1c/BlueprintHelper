#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperActionResolutionSettings
{
	int32 CandidateCount = 0;
	FString DefaultSearchMode;
	FString DefaultAmbiguityPolicy;
};

class BLUEPRINTHELPER_API FBlueprintHelperActionResolutionSettingsResolver
{
public:
	static FBlueprintHelperActionResolutionSettings Load();
};
