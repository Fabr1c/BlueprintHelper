// BlueprintHelper call_function resolver utility helpers.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

class FProperty;
class UBlueprint;
class UClass;
class UEdGraph;
class UFunction;
class UScriptStruct;

class FBlueprintHelperCallFunctionResolverUtils
{
public:
	static FString NormalizeForCompare(const FString& Value);
	static FString NormalizeCompactForCompare(const FString& Value);
	static bool CompactEquals(const FString& Left, const FString& Right);
	static FString GetOwnerClassPath(const UFunction* Function);
	static FString GetOwnerClassName(const UFunction* Function);
	static FString GetFunctionDisplayName(const UFunction* Function);
	static FString GetFunctionCategory(const UFunction* Function);
	static FString NormalizeTypeToken(const FString& Type);
	static UClass* ResolveClassByTypeName(const FString& TypeName);
	static UScriptStruct* ResolveStructByTypeName(const FString& TypeName);
	static bool IsBlueprintCallableFunction(const UFunction* Function);
	static bool IsUsableOwnerClass(const UClass* Class);
	static bool IsGraphCompatible(const UFunction* Function, UBlueprint* Blueprint, UEdGraph* Graph);
	static bool IsFunctionInputProperty(const FProperty* Property);
	static bool IsFunctionOutputProperty(const FProperty* Property);
	static FString GetPropertySemanticType(const FProperty* Property);
	static FProperty* FindInputPropertyByName(const UFunction* Function, const FString& PinName);
	static bool IsNumericSemanticType(const FString& Type);
	static bool IsSemanticTypeCompatibleWithProperty(const FString& Type, const FProperty* Property);
	static bool IsPinTypeCompatibleWithProperty(const FBlueprintHelperCallFunctionPinType& PinType, const FProperty* Property);
	static bool DoesGraphSupportImpureFunctions(UEdGraph* Graph);
	static FBlueprintHelperK2CallContext BuildEffectiveContext(
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool IsTargetObjectTypeCompatible(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool IsExpectedReturnTypeCompatible(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool AreRequestedArgumentsCompatible(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static FString DescribeCandidateMismatch(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool PassesMetadataFilters(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static int32 ComputeTypedConstraintBonus(const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool OwnerMatches(const FBlueprintHelperCallFunctionCandidate& Candidate, const FString& OwnerQuery);
	static void TokenizeSearchText(const FString& Text, TArray<FString>& OutTokens);
	static FString BuildSearchText(const FBlueprintHelperCallFunctionCandidate& Candidate);
	static bool AllQueryTokensContained(const FString& Query, const FString& SearchText);
	static bool HasExactToken(const FString& Query, const FString& SearchText);
	static void AddCandidateForFunction(
		UFunction* Function,
		const FBlueprintHelperCallFunctionResolveRequest& Request,
		TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates,
		UBlueprintNodeSpawner* NodeSpawner = nullptr);
	static void AddActionDatabaseCandidates(
		const FBlueprintHelperCallFunctionResolveRequest& Request,
		TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates);
	static TArray<FBlueprintHelperCallFunctionCandidate> BuildCandidateUniverse(
		const FBlueprintHelperCallFunctionResolveRequest& Request);
	static int32 ApplyCategoryPriorityBonus(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const TArray<FString>& CategoryPriority);
	static bool IsExactSearchMode(const FString& SearchMode);
	static int32 ScoreCandidate(
		const FBlueprintHelperCallFunctionCandidate& Candidate,
		const FString& Query,
		const FString& QualifiedOwner,
		const FString& QualifiedFunction,
		const FBlueprintHelperCallFunctionResolveRequest& Request,
		FString& OutMatchReason);
	static void SortCandidates(TArray<FBlueprintHelperCallFunctionCandidate>& Candidates);
	static FString BuildCandidateSummary(const FBlueprintHelperCallFunctionCandidate& Candidate);
	static FString JsonQuote(const FString& Value);
	static FString BuildCandidateFunctionJsonString(const FBlueprintHelperCallFunctionCandidate& Candidate);
	static FBlueprintHelperCallFunctionCandidateInfo BuildCandidateFunctionInfo(
		const FBlueprintHelperCallFunctionCandidate& Candidate);
	static FString BuildCandidateListMessage(
		const FString& Prefix,
		const FString& Query,
		const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates);
	static void SetTopCandidates(
		FBlueprintHelperCallFunctionResolveResult& Result,
		const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
		int32 MaxCandidates);
	static bool HasOwnerCandidate(
		const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
		const FString& OwnerQuery);
	static bool HasNativeDisplayConflict(const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates);
	static void PreferGeneratedClassOverSkeletonDuplicates(
		TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
		const UBlueprint* Blueprint);
	static void PreferTargetBlueprintCandidates(
		TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
		const UBlueprint* Blueprint);
};
