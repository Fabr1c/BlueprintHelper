#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperOverrideEventResolver.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "UObject/FieldIterator.h"

FBlueprintHelperOverrideEventResolveResult FBlueprintHelperOverrideEventResolver::Resolve(
	UBlueprint* Blueprint,
	const FString& RequestedEventName) const
{
	FBlueprintHelperOverrideEventResolveResult Result;

	UClass* CandidateClass = Blueprint ? Blueprint->ParentClass : nullptr;
	if (!CandidateClass && Blueprint && Blueprint->GeneratedClass)
	{
		CandidateClass = Blueprint->GeneratedClass->GetSuperClass();
	}
	if (!CandidateClass && Blueprint)
	{
		CandidateClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->SkeletonGeneratedClass;
	}

	if (!CandidateClass)
	{
		Result.Message = TEXT("Unable to resolve parent class for override/native event candidate collection.");
		return Result;
	}

	CollectCandidates(CandidateClass, Result.Candidates);

	const FString NormalizedRequest = NormalizeToken(RequestedEventName);
	if (NormalizedRequest.IsEmpty())
	{
		Result.Message = TEXT("Override/native event name is empty.");
		return Result;
	}

	TArray<FBlueprintHelperOverrideEventCandidate> Matches;
	for (const FBlueprintHelperOverrideEventCandidate& Candidate : Result.Candidates)
	{
		if (CandidateMatches(Candidate, NormalizedRequest))
		{
			Matches.Add(Candidate);
		}
	}

	if (Matches.Num() == 1)
	{
		Result.bResolved = true;
		Result.ResolvedEventName = Matches[0].FunctionName;
		Result.Message = FString::Printf(TEXT("Resolved override/native event: %s."), *Result.ResolvedEventName.ToString());
		return Result;
	}

	if (Matches.Num() > 1)
	{
		Result.bAmbiguous = true;
		Result.Message = FString::Printf(
			TEXT("Override/native event name is ambiguous: %s."),
			*RequestedEventName);
		return Result;
	}

	Result.Message = FString::Printf(
		TEXT("Override/native event function not found: %s."),
		*RequestedEventName);
	return Result;
}

FString FBlueprintHelperOverrideEventResolver::NormalizeToken(const FString& Value)
{
	FString Normalized;
	const FString Lower = Value.ToLower();
	for (const TCHAR Character : Lower)
	{
		if (FChar::IsAlnum(Character))
		{
			Normalized.AppendChar(Character);
		}
	}

	const TCHAR* Prefixes[] = {
		TEXT("receive"),
		TEXT("event"),
	};

	bool bStrippedPrefix = true;
	while (bStrippedPrefix)
	{
		bStrippedPrefix = false;
		for (const TCHAR* Prefix : Prefixes)
		{
			const FString PrefixString(Prefix);
			if (Normalized.StartsWith(PrefixString) && Normalized.Len() > PrefixString.Len())
			{
				Normalized = Normalized.RightChop(PrefixString.Len());
				bStrippedPrefix = true;
				break;
			}
		}
	}

	return Normalized;
}

void FBlueprintHelperOverrideEventResolver::CollectCandidates(
	UClass* Class,
	TArray<FBlueprintHelperOverrideEventCandidate>& OutCandidates)
{
	TSet<FName> SeenFunctionNames;
	for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (!Function || !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
		{
			continue;
		}

		const FName FunctionName = Function->GetFName();
		if (SeenFunctionNames.Contains(FunctionName))
		{
			continue;
		}
		SeenFunctionNames.Add(FunctionName);

		FBlueprintHelperOverrideEventCandidate Candidate;
		Candidate.FunctionName = FunctionName;
		Candidate.DisplayName = UEdGraphSchema_K2::GetFriendlySignatureName(Function).ToString();
		if (const UClass* OwnerClass = Function->GetOwnerClass())
		{
			Candidate.OwnerClassPath = OwnerClass->GetPathName();
		}
		Candidate.bPlaceableAsEvent = true;
		OutCandidates.Add(Candidate);
	}
}

bool FBlueprintHelperOverrideEventResolver::CandidateMatches(
	const FBlueprintHelperOverrideEventCandidate& Candidate,
	const FString& NormalizedRequest)
{
	if (NormalizedRequest.IsEmpty())
	{
		return false;
	}

	return NormalizeToken(Candidate.FunctionName.ToString()) == NormalizedRequest ||
		NormalizeToken(Candidate.DisplayName) == NormalizedRequest;
}
