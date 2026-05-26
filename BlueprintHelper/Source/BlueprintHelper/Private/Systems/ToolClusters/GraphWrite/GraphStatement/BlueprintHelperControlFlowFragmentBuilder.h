#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperControlFlowFragmentBuilder
{
public:
	static bool SupportsOperation(const FString& Operation);
	static TArray<FString> RequiredEvidenceKeys(const FString& Operation);
};
