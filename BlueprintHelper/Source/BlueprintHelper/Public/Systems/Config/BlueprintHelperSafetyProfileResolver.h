// BlueprintHelper shared safety profile resolver

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperRuntimeProfileTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperSafetyProfileResolver
{
public:
	static EBlueprintHelperSafetyProfile ResolveSafetyProfile();
	static FString ResolveSafetyProfileString();
	static bool IsAutoRepair();
};
