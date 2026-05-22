#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/FieldIterator.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

FBlueprintHelperCallFunctionResolveResult FBlueprintHelperCallFunctionResolver::Resolve(
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	FBlueprintHelperCallFunctionResolveResult Result;
	const FString Query = Request.Query.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Blocked;
		Result.ErrorCode = TEXT("function_call_not_found");
		Result.Message = TEXT("call_function.name is empty.");
		return Result;
	}

	FString QualifiedOwner;
	FString QualifiedFunction;
	const bool bQualifiedQuery = TryParseQualifiedQuery(Query, QualifiedOwner, QualifiedFunction);
	const bool bPickBest = Request.AmbiguityPolicy.Equals(TEXT("pick_best"), ESearchCase::IgnoreCase)
		|| Request.AmbiguityPolicy.Equals(TEXT("best"), ESearchCase::IgnoreCase);
	const FBlueprintHelperK2CallContext EffectiveContext =
		FBlueprintHelperCallFunctionResolverUtils::BuildEffectiveContext(Request);

	TArray<FBlueprintHelperCallFunctionCandidate> Candidates = FBlueprintHelperCallFunctionResolverUtils::BuildCandidateUniverse(Request);
	if (bQualifiedQuery && !FBlueprintHelperCallFunctionResolverUtils::HasOwnerCandidate(Candidates, QualifiedOwner))
	{
		if (UClass* QualifiedOwnerClass = FBlueprintHelperCallFunctionResolverUtils::ResolveClassByTypeName(QualifiedOwner))
		{
			TSet<FString> ExistingStableIds;
			for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
			{
				ExistingStableIds.Add(Candidate.StableId);
			}

			TMap<FString, FBlueprintHelperCallFunctionCandidate> QualifiedCandidates;
			for (UClass* Class = QualifiedOwnerClass; Class; Class = Class->GetSuperClass())
			{
				if (!FBlueprintHelperCallFunctionResolverUtils::IsUsableOwnerClass(Class))
				{
					continue;
				}

				for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
				{
					FBlueprintHelperCallFunctionResolverUtils::AddCandidateForFunction(*FuncIt, Request, QualifiedCandidates);
				}
			}

			TArray<FBlueprintHelperCallFunctionCandidate> OwnerCandidates;
			QualifiedCandidates.GenerateValueArray(OwnerCandidates);
			for (FBlueprintHelperCallFunctionCandidate& Candidate : OwnerCandidates)
			{
				if (!ExistingStableIds.Contains(Candidate.StableId))
				{
					Candidates.Add(MoveTemp(Candidate));
				}
			}
		}
	}

	if (bQualifiedQuery && !FBlueprintHelperCallFunctionResolverUtils::HasOwnerCandidate(Candidates, QualifiedOwner) && !QualifiedOwner.StartsWith(TEXT("/Script/")))
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Blocked;
		Result.ErrorCode = TEXT("explicit_member_call_not_supported");
		Result.Message = TEXT("call_function.name uses an explicit member prefix; first slice supports graph/self/library calls only.");
		return Result;
	}

	int32 MetadataCompatibleCandidateCount = 0;
	int32 TextMatchedCandidateCount = 0;
	int32 GraphRejectedCandidateCount = 0;
	FString FirstTextMatchedCandidate;
	TArray<FBlueprintHelperCallFunctionCandidate> DiagnosticCandidates;
	TArray<FBlueprintHelperCallFunctionCandidate> ScoredCandidates;
	for (FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		FString MatchReason;
		const int32 Score = FBlueprintHelperCallFunctionResolverUtils::ScoreCandidate(Candidate, Query, QualifiedOwner, QualifiedFunction, Request, MatchReason);
		if (Score <= 0)
		{
			continue;
		}
		++TextMatchedCandidateCount;
		Candidate.Score = Score;
		Candidate.MatchReason = MatchReason;
		Candidate.MismatchReason = FBlueprintHelperCallFunctionResolverUtils::DescribeCandidateMismatch(Candidate, Request);
		if (FirstTextMatchedCandidate.IsEmpty())
		{
			const bool bCallable = Candidate.bBlueprintCallable || Candidate.bBlueprintPure;
			const bool bImpureGraphOk = !(Candidate.bLatent || (!Candidate.bBlueprintPure && Candidate.bBlueprintCallable))
				|| FBlueprintHelperCallFunctionResolverUtils::DoesGraphSupportImpureFunctions(EffectiveContext.Graph);
			const bool bTargetOk = Candidate.MismatchReason.IsEmpty()
				|| !Candidate.MismatchReason.StartsWith(TEXT("target_object_type_mismatch"));
			const bool bArgsOk = Candidate.MismatchReason.IsEmpty()
				|| (!Candidate.MismatchReason.StartsWith(TEXT("missing_argument_pin"))
					&& !Candidate.MismatchReason.StartsWith(TEXT("argument_type_mismatch"))
					&& !Candidate.MismatchReason.StartsWith(TEXT("argument_pin_type_mismatch")));
			FirstTextMatchedCandidate = FString::Printf(
				TEXT("%s inputs=[%s] callable=%d impure_graph_ok=%d target_ok=%d args_ok=%d mismatch=\"%s\""),
				*Candidate.StableId,
				*FString::Join(Candidate.InputPins, TEXT(",")),
				bCallable ? 1 : 0,
				bImpureGraphOk ? 1 : 0,
				bTargetOk ? 1 : 0,
				bArgsOk ? 1 : 0,
				*Candidate.MismatchReason);
		}

		if (DiagnosticCandidates.Num() < Request.MaxCandidates)
		{
			DiagnosticCandidates.Add(Candidate);
		}

		if (!FBlueprintHelperCallFunctionResolverUtils::PassesMetadataFilters(Candidate, Request))
		{
			continue;
		}
		++MetadataCompatibleCandidateCount;

		Candidate.Score = Score;
		Candidate.MatchReason = MatchReason;
		if (Candidate.bGraphCompatible && Candidate.NodeSpawner.IsValid())
		{
			ScoredCandidates.Add(Candidate);
		}
		else
		{
			++GraphRejectedCandidateCount;
		}
	}

	if (!bQualifiedQuery)
	{
		FBlueprintHelperCallFunctionResolverUtils::PreferTargetBlueprintCandidates(ScoredCandidates, EffectiveContext.Blueprint);
		FBlueprintHelperCallFunctionResolverUtils::PreferGeneratedClassOverSkeletonDuplicates(ScoredCandidates, EffectiveContext.Blueprint);
	}
	FBlueprintHelperCallFunctionResolverUtils::SortCandidates(ScoredCandidates);
	FBlueprintHelperCallFunctionResolverUtils::SetTopCandidates(Result, ScoredCandidates, Request.MaxCandidates);

	if (ScoredCandidates.Num() == 0)
	{
		FBlueprintHelperCallFunctionResolverUtils::SortCandidates(DiagnosticCandidates);
		FBlueprintHelperCallFunctionResolverUtils::SetTopCandidates(Result, DiagnosticCandidates, Request.MaxCandidates);
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::NotFound;
		Result.ErrorCode = TEXT("function_call_not_found");
		Result.Message = FString::Printf(
			TEXT("call_function resolve failed: no graph-compatible function matched '%s' (candidates=%d metadata_compatible=%d text_matched=%d graph_rejected=%d first_text_match=\"%s\")."),
			*Query,
			Candidates.Num(),
			MetadataCompatibleCandidateCount,
			TextMatchedCandidateCount,
			GraphRejectedCandidateCount,
			*FirstTextMatchedCandidate);
		return Result;
	}

	if (!bQualifiedQuery && FBlueprintHelperCallFunctionResolverUtils::HasNativeDisplayConflict(ScoredCandidates))
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = ScoredCandidates[0];
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *ScoredCandidates[0].StableId);
			return Result;
		}

		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
		Result.ErrorCode = TEXT("ambiguous_function_call");
		Result.Message = FBlueprintHelperCallFunctionResolverUtils::BuildCandidateListMessage(
			FString::Printf(TEXT("call_function resolve failed: native and display-name matches conflict for '%s'."), *Query),
			Query,
			Result.Candidates);
		return Result;
	}

	const FBlueprintHelperCallFunctionCandidate& Top = ScoredCandidates[0];
	if (Top.Score >= 850)
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
			return Result;
		}

		for (int32 Index = 1; Index < ScoredCandidates.Num(); ++Index)
		{
			const FBlueprintHelperCallFunctionCandidate& Candidate = ScoredCandidates[Index];
			if (Candidate.Score == Top.Score && Candidate.StableId != Top.StableId)
			{
				Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
				Result.ErrorCode = TEXT("ambiguous_function_call");
				Result.Message = FBlueprintHelperCallFunctionResolverUtils::BuildCandidateListMessage(
					FString::Printf(TEXT("call_function resolve failed: '%s' is ambiguous."), *Query),
					Query,
					Result.Candidates);
				return Result;
			}
		}

		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
		Result.Selected = Top;
		Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s."), *Query, *Top.StableId);
		return Result;
	}

	if (Top.Score >= 700)
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
			return Result;
		}

		int32 CompatibleAboveZero = 0;
		for (const FBlueprintHelperCallFunctionCandidate& Candidate : ScoredCandidates)
		{
			if (Candidate.Score > 0)
			{
				++CompatibleAboveZero;
			}
		}

		if (CompatibleAboveZero == 1)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s."), *Query, *Top.StableId);
			return Result;
		}
	}

	if (bPickBest)
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
		Result.Selected = Top;
		Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
		return Result;
	}

	Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
	Result.ErrorCode = TEXT("ambiguous_function_call");
	Result.Message = FBlueprintHelperCallFunctionResolverUtils::BuildCandidateListMessage(
		FString::Printf(TEXT("call_function resolve failed: '%s' did not identify a unique function."), *Query),
		Query,
		Result.Candidates);
	return Result;
}

FString FBlueprintHelperCallFunctionResolver::MakeStableId(const UFunction* Function)
{
	const FString OwnerClassPath = FBlueprintHelperCallFunctionResolverUtils::GetOwnerClassPath(Function);
	return Function && !OwnerClassPath.IsEmpty()
		? FString::Printf(TEXT("%s:%s"), *OwnerClassPath, *Function->GetName())
		: FString();
}

bool FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(
	const FString& Query,
	FString& OutOwner,
	FString& OutFunction)
{
	OutOwner.Reset();
	OutFunction.Reset();

	const FString Trimmed = Query.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	int32 ColonIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), ColonIndex) && ColonIndex > 0 && ColonIndex < Trimmed.Len() - 1)
	{
		OutOwner = Trimmed.Left(ColonIndex).TrimStartAndEnd();
		OutFunction = Trimmed.Mid(ColonIndex + 1).TrimStartAndEnd();
		return !OutOwner.IsEmpty() && !OutFunction.IsEmpty();
	}

	int32 DotIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT('.'), DotIndex) && DotIndex > 0 && DotIndex < Trimmed.Len() - 1)
	{
		OutOwner = Trimmed.Left(DotIndex).TrimStartAndEnd();
		OutFunction = Trimmed.Mid(DotIndex + 1).TrimStartAndEnd();
		return !OutOwner.IsEmpty() && !OutFunction.IsEmpty();
	}

	return false;
}
