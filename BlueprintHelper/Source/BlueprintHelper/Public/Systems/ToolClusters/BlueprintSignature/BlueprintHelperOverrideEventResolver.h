#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UClass;
class UFunction;

struct BLUEPRINTHELPER_API FBlueprintHelperOverrideEventCandidate
{
	FName FunctionName;
	FString DisplayName;
	FString OwnerClassPath;
	FString CandidateSource;
	TArray<FString> MatchAliases;
	bool bPlaceableAsEvent = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperOverrideEventResolveRequest
{
	UBlueprint* Blueprint = nullptr;
	FString RequestedEventName;
	TArray<UClass*> AdditionalCandidateClasses;
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
	FBlueprintHelperOverrideEventResolveResult Resolve(const FBlueprintHelperOverrideEventResolveRequest& Request) const;
	FBlueprintHelperOverrideEventResolveResult Resolve(UBlueprint* Blueprint, const FString& RequestedEventName) const;

private:
	static FString NormalizeToken(const FString& Value);
	static FString MakeCandidateSource(UClass* Class);
	static void AddCandidateClass(UClass* Class, TArray<UClass*>& OutClasses, TSet<UClass*>& SeenClasses);
	static void CollectCandidateClasses(
		const FBlueprintHelperOverrideEventResolveRequest& Request,
		TArray<UClass*>& OutClasses);
	static void AddAlias(TArray<FString>& Aliases, const FString& Value);
	static TArray<FString> BuildMatchAliases(const UFunction* Function, const FString& DisplayName);
	static void CollectCandidates(
		UClass* Class,
		TSet<FName>& SeenFunctionNames,
		TArray<FBlueprintHelperOverrideEventCandidate>& OutCandidates);
	static bool CandidateMatches(const FBlueprintHelperOverrideEventCandidate& Candidate, const FString& NormalizedRequest);
};
