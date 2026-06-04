#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class UClass;

struct FBlueprintHelperComponentClassResolveRequest
{
	FString RawClass;
	UClass* ExpectedBaseClass = nullptr;
	bool bAllowBlueprintGeneratedClass = true;
	bool bAllowEngineShortName = true;
	bool bAllowComponentSuffixFallback = true;
};

struct FBlueprintHelperComponentClassResolveResult
{
	UClass* ResolvedClass = nullptr;
	FString ResolvedClassPath;
	FString ErrorCode;
	FString ErrorMessage;
	TArray<FString> AttemptedPaths;
};

class BLUEPRINTHELPER_API FBlueprintHelperComponentClassResolver
{
public:
	static bool Resolve(
		const FBlueprintHelperComponentClassResolveRequest& Request,
		FBlueprintHelperComponentClassResolveResult& OutResult);

	static bool ResolveActorComponentClass(
		const FString& RawClass,
		FBlueprintHelperComponentClassResolveResult& OutResult);
};
