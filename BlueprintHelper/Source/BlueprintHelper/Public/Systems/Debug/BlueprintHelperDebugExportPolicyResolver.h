// BlueprintHelper debug export policy resolver.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperDebugExportPolicy
{
	FString ExportProfile = TEXT("standard");
	bool bContainsFullSettings = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperDebugExportPolicyResolver
{
public:
	static FBlueprintHelperDebugExportPolicy Load();
};
