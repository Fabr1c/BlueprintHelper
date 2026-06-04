#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentClassResolver.h"

#include "Components/ActorComponent.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

class FBlueprintHelperComponentClassResolverLocalUtils
{
public:
	static FString NormalizeRawClass(FString RawClass)
	{
		RawClass.TrimStartAndEndInline();
		RawClass.RemoveFromStart(TEXT("class "), ESearchCase::IgnoreCase);
		RawClass.TrimStartAndEndInline();
		RawClass.TrimQuotesInline();
		return RawClass;
	}

	static void AddUniqueCandidate(TArray<FString>& Candidates, const FString& Candidate)
	{
		if (!Candidate.IsEmpty())
		{
			Candidates.AddUnique(Candidate);
		}
	}

	static void AddGamePathCandidates(TArray<FString>& Candidates, const FString& RawClass)
	{
		if (!RawClass.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(RawClass))
		{
			const FString ShortName = FPackageName::GetShortName(RawClass);
			AddUniqueCandidate(Candidates, FString::Printf(TEXT("%s.%s_C"), *RawClass, *ShortName));
			AddUniqueCandidate(Candidates, FString::Printf(TEXT("%s.%s"), *RawClass, *ShortName));
			AddUniqueCandidate(Candidates, RawClass);
			return;
		}

		AddUniqueCandidate(Candidates, RawClass);

		if (RawClass.Contains(TEXT(".")) && !RawClass.EndsWith(TEXT("_C")))
		{
			AddUniqueCandidate(Candidates, RawClass + TEXT("_C"));
		}
	}

	static void BuildCandidatePaths(
		const FBlueprintHelperComponentClassResolveRequest& Request,
		const FString& RawClass,
		TArray<FString>& OutCandidates)
	{
		if (RawClass.StartsWith(TEXT("/Game/")))
		{
			if (Request.bAllowBlueprintGeneratedClass)
			{
				AddGamePathCandidates(OutCandidates, RawClass);
			}
			else
			{
				AddUniqueCandidate(OutCandidates, RawClass);
			}
			return;
		}

		if (RawClass.StartsWith(TEXT("/Script/")) || RawClass.Contains(TEXT(".")))
		{
			AddUniqueCandidate(OutCandidates, RawClass);
			return;
		}

		if (Request.bAllowEngineShortName)
		{
			AddUniqueCandidate(OutCandidates, FString::Printf(TEXT("/Script/Engine.%s"), *RawClass));
			if (Request.bAllowComponentSuffixFallback && !RawClass.EndsWith(TEXT("Component")))
			{
				AddUniqueCandidate(OutCandidates, FString::Printf(TEXT("/Script/Engine.%sComponent"), *RawClass));
			}
		}
	}

	static UClass* LoadClassCandidate(const FString& CandidatePath)
	{
		if (CandidatePath.IsEmpty())
		{
			return nullptr;
		}

		if (UClass* ExistingClass = FindObject<UClass>(nullptr, *CandidatePath))
		{
			return ExistingClass;
		}
		if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *CandidatePath))
		{
			return LoadedClass;
		}
		return LoadClass<UObject>(nullptr, *CandidatePath);
	}
};

bool FBlueprintHelperComponentClassResolver::Resolve(
	const FBlueprintHelperComponentClassResolveRequest& Request,
	FBlueprintHelperComponentClassResolveResult& OutResult)
{
	OutResult = FBlueprintHelperComponentClassResolveResult();
	const FString RawClass = FBlueprintHelperComponentClassResolverLocalUtils::NormalizeRawClass(Request.RawClass);
	if (RawClass.IsEmpty())
	{
		OutResult.ErrorCode = TEXT("unsupported_component_class");
		OutResult.ErrorMessage = TEXT("component class is empty");
		return false;
	}

	TArray<FString> CandidatePaths;
	FBlueprintHelperComponentClassResolverLocalUtils::BuildCandidatePaths(Request, RawClass, CandidatePaths);
	OutResult.AttemptedPaths = CandidatePaths;

	UClass* ExpectedBaseClass = Request.ExpectedBaseClass ? Request.ExpectedBaseClass : UActorComponent::StaticClass();
	FString FirstUnsupportedClassPath;
	for (const FString& CandidatePath : CandidatePaths)
	{
		UClass* CandidateClass = FBlueprintHelperComponentClassResolverLocalUtils::LoadClassCandidate(CandidatePath);
		if (!CandidateClass)
		{
			continue;
		}

		if (!CandidateClass->IsChildOf(ExpectedBaseClass))
		{
			if (FirstUnsupportedClassPath.IsEmpty())
			{
				FirstUnsupportedClassPath = CandidateClass->GetPathName();
			}
			continue;
		}

		OutResult.ResolvedClass = CandidateClass;
		OutResult.ResolvedClassPath = CandidateClass->GetPathName();
		return true;
	}

	OutResult.ErrorCode = TEXT("unsupported_component_class");
	OutResult.ErrorMessage = FirstUnsupportedClassPath.IsEmpty()
		? FString::Printf(TEXT("unable to resolve component class: %s"), *RawClass)
		: FString::Printf(
			TEXT("%s is not a child of %s"),
			*FirstUnsupportedClassPath,
			*ExpectedBaseClass->GetPathName());
	return false;
}

bool FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(
	const FString& RawClass,
	FBlueprintHelperComponentClassResolveResult& OutResult)
{
	FBlueprintHelperComponentClassResolveRequest Request;
	Request.RawClass = RawClass;
	Request.ExpectedBaseClass = UActorComponent::StaticClass();
	return Resolve(Request, OutResult);
}
