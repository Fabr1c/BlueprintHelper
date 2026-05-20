#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperGraphLayoutRuleSourceResolver
{
public:
	static FString ResolveRuleSourcePath();
	static FString GetDefaultRuleSourcePath();

private:
	static bool IsAllowedAbsolutePath(const FString& AbsolutePath);
	static FString NormalizeAbsolutePath(const FString& Path);
	static bool IsUnderDirectory(const FString& CandidatePath, const FString& RootDirectory);
};
