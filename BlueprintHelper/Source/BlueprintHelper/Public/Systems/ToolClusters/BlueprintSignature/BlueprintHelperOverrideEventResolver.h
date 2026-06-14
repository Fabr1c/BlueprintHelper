#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UFunction;

struct BLUEPRINTHELPER_API FBlueprintHelperOverrideEventCandidate
{
	FName FunctionName;
	FString DisplayName;
	FString OwnerClassPath;
	bool bPlaceableAsEvent = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperOverrideEventResolveResult
{
	bool bResolved = false;
	bool bAmbiguous = false;
	FName ResolvedEventName;
	TArray<FBlueprintHelperOverrideEventCandidate> Candidates;
	FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperOverrideEventResolver
{
public:
	FBlueprintHelperOverrideEventResolveResult Resolve(UBlueprint* Blueprint, const FString& RequestedEventName) const;

private:
	static FString NormalizeToken(const FString& Value);
	static void CollectCandidates(UClass* Class, TArray<FBlueprintHelperOverrideEventCandidate>& OutCandidates);
	static bool CandidateMatches(const FBlueprintHelperOverrideEventCandidate& Candidate, const FString& NormalizedRequest);
};
