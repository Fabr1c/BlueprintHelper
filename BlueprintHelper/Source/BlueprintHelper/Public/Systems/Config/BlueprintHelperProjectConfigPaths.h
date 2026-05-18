#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperProjectConfigPaths
{
public:
	static FString GetProjectConfigDir();
	static FString GetAgentProfilePath();
	static FString GetGraphLayoutRulesPath();
};
