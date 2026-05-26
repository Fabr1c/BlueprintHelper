// Shared DTOs for CallFunction candidate diagnostics.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperCallFunctionCandidateInfo
{
	FString StableId;
	FString DisplayName;
	FString OwnerClassPath;
	FString NativeFunctionName;
	FString Category;
	FString NodeClassPath;
	FString MatchReason;
	FString ReturnType;
	FString WorldContextPin;
	FString TargetObjectPin;
	FString CapabilityId;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClassPath;
	TArray<FString> InputPins;
	TMap<FString, FString> InputPinTypes;
	TMap<FString, FString> CapabilityFacts;
	TMap<FString, FString> ReadbackFacts;
	FString MismatchReason;
	int32 Score = 0;
	bool bGraphCompatible = false;
	bool bFromActionDatabase = false;
	bool bBlueprintCallable = false;
	bool bBlueprintPure = false;
	bool bLatent = false;
	bool bRequiresWorldContext = false;
	bool bCustomThunk = false;
	bool bHasArrayParm = false;
	bool bHasArrayTypeDependentParams = false;
	bool bDeterminesOutputType = false;
};
