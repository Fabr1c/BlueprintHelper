#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperMacroControlFragmentBuilder
{
public:
	static bool SupportsOperation(const FString& Operation);
	static TArray<FString> RequiredEvidenceKeys();
};
