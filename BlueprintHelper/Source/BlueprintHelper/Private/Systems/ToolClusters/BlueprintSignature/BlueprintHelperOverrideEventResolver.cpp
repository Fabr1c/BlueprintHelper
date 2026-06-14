#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperOverrideEventResolver.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/FieldIterator.h"

FBlueprintHelperOverrideEventResolveResult FBlueprintHelperOverrideEventResolver::Resolve(
	UBlueprint* Blueprint,
	const FString& RequestedEventName) const
{
	FBlueprintHelperOverrideEventResolveRequest Request;
	Request.Blueprint = Blueprint;
	Request.RequestedEventName = RequestedEventName;
	return Resolve(Request);
}

FBlueprintHelperOverrideEventResolveResult FBlueprintHelperOverrideEventResolver::Resolve(
	const FBlueprintHelperOverrideEventResolveRequest& Request) const
{
	FBlueprintHelperOverrideEventResolveResult Result;

	TArray<UClass*> CandidateClasses;
	CollectCandidateClasses(Request, CandidateClasses);
	if (CandidateClasses.IsEmpty())
	{
		Result.Message = TEXT("Unable to resolve any candidate class for override/native event candidate collection.");
		return Result;
	}

	TSet<FName> SeenFunctionNames;
	for (UClass* CandidateClass : CandidateClasses)
	{
		CollectCandidates(CandidateClass, SeenFunctionNames, Result.Candidates);
	}

	const FString NormalizedRequest = NormalizeToken(Request.RequestedEventName);
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
			*Request.RequestedEventName);
		return Result;
	}

	Result.Message = FString::Printf(
		TEXT("Override/native event function not found: %s."),
		*Request.RequestedEventName);
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
		TEXT("k2"),
		TEXT("bp"),
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

FString FBlueprintHelperOverrideEventResolver::MakeCandidateSource(UClass* Class)
{
	return Class ? Class->GetPathName() : FString();
}

void FBlueprintHelperOverrideEventResolver::AddCandidateClass(
	UClass* Class,
	TArray<UClass*>& OutClasses,
	TSet<UClass*>& SeenClasses)
{
	if (!Class || SeenClasses.Contains(Class))
	{
		return;
	}
	SeenClasses.Add(Class);
	OutClasses.Add(Class);
}

void FBlueprintHelperOverrideEventResolver::CollectCandidateClasses(
	const FBlueprintHelperOverrideEventResolveRequest& Request,
	TArray<UClass*>& OutClasses)
{
	TSet<UClass*> SeenClasses;
	UBlueprint* const Blueprint = Request.Blueprint;

	AddCandidateClass(Blueprint ? Blueprint->ParentClass : nullptr, OutClasses, SeenClasses);
	AddCandidateClass(Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetSuperClass() : nullptr, OutClasses, SeenClasses);
	AddCandidateClass(Blueprint ? Blueprint->SkeletonGeneratedClass : nullptr, OutClasses, SeenClasses);
	AddCandidateClass(Blueprint ? Blueprint->GeneratedClass : nullptr, OutClasses, SeenClasses);

	if (Blueprint)
	{
		TArray<UClass*> InterfaceClasses;
		FBlueprintEditorUtils::FindImplementedInterfaces(Blueprint, /*bGetAllInterfaces=*/ true, InterfaceClasses);
		for (UClass* InterfaceClass : InterfaceClasses)
		{
			AddCandidateClass(InterfaceClass, OutClasses, SeenClasses);
		}
	}

	for (UClass* AdditionalClass : Request.AdditionalCandidateClasses)
	{
		AddCandidateClass(AdditionalClass, OutClasses, SeenClasses);
	}
}

void FBlueprintHelperOverrideEventResolver::AddAlias(TArray<FString>& Aliases, const FString& Value)
{
	const FString Trimmed = Value.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	if (!Aliases.Contains(Trimmed))
	{
		Aliases.Add(Trimmed);
	}

	for (const TCHAR* Prefix : { TEXT("Receive"), TEXT("Event"), TEXT("K2_"), TEXT("BP_") })
	{
		const FString PrefixString(Prefix);
		if (Trimmed.StartsWith(PrefixString) && Trimmed.Len() > PrefixString.Len())
		{
			const FString WithoutPrefix = Trimmed.RightChop(PrefixString.Len());
			if (!WithoutPrefix.IsEmpty() && !Aliases.Contains(WithoutPrefix))
			{
				Aliases.Add(WithoutPrefix);
			}
		}
	}
}

TArray<FString> FBlueprintHelperOverrideEventResolver::BuildMatchAliases(
	const UFunction* Function,
	const FString& DisplayName)
{
	TArray<FString> Aliases;
	if (!Function)
	{
		return Aliases;
	}

	AddAlias(Aliases, Function->GetName());
	AddAlias(Aliases, DisplayName);
	for (const FName MetadataKey : {
		FName(TEXT("DisplayName")),
		FName(TEXT("ScriptName")),
		FName(TEXT("CompactNodeTitle")),
		FName(TEXT("FriendlyName")),
	})
	{
		AddAlias(Aliases, Function->GetMetaData(MetadataKey));
	}
	return Aliases;
}

void FBlueprintHelperOverrideEventResolver::CollectCandidates(
	UClass* Class,
	TSet<FName>& SeenFunctionNames,
	TArray<FBlueprintHelperOverrideEventCandidate>& OutCandidates)
{
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
		Candidate.CandidateSource = MakeCandidateSource(Class);
		Candidate.MatchAliases = BuildMatchAliases(Function, Candidate.DisplayName);
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

	if (NormalizeToken(Candidate.FunctionName.ToString()) == NormalizedRequest ||
		NormalizeToken(Candidate.DisplayName) == NormalizedRequest)
	{
		return true;
	}

	for (const FString& Alias : Candidate.MatchAliases)
	{
		if (NormalizeToken(Alias) == NormalizedRequest)
		{
			return true;
		}
	}
	return false;
}
