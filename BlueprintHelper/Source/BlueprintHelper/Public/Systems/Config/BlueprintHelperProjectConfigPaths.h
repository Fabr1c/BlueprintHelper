#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperProjectConfigPaths
{
public:
	static FString GetProjectConfigDir();
	static FString GetProjectProfilePath();
	static FString GetAgentProfilePath();
	static FString GetGraphLayoutRulesPath();
	static FString GetProjectSettingPath();
	static FString GetUserSettingOverridePath();
};
