#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperActionResolutionSettings
{
	int32 CandidateCount = 0;
	FString DefaultSearchMode;
	FString DefaultAmbiguityPolicy;
	int32 AutoSearchMaxCandidatesPerStatement = 3;
	int32 AutoSearchMaxStatements = 16;
	int32 AutoSearchMaxTotalMs = 120;
	FString AutoSearchDetailLevel = TEXT("short");
};

class BLUEPRINTHELPER_API FBlueprintHelperActionResolutionSettingsResolver
{
public:
	static FBlueprintHelperActionResolutionSettings Load();
};
