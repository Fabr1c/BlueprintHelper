// BlueprintHelper GraphStatement expression/type utility helpers.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

struct FBlueprintHelperGraphExpressionIR;

class FBlueprintHelperGraphStatementTypeUtils
{
public:
	static FString MakeExpressionFragmentId(const FBlueprintHelperGraphExpressionIR& Expression);
	static FString ResolveCompareOperatorFunctionName(const FBlueprintHelperGraphExpressionIR& Expression);

private:
	static void AddUniqueString(TArray<FString>& Values, const FString& Value);
	static FString SanitizeFragmentIdPart(const FString& Value);
	static FString ResolveExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind);
	static FString NormalizeCompareOperatorToken(const FString& Operator);
	static bool TokenMatches(const FString& Token, const TArray<const TCHAR*>& Candidates);
	static FString ResolveCompareOperatorBaseName(const FString& Operator);
	static FString NormalizeCompareTypeToken(const FString& Type);
	static bool TypeTokenMatches(const FString& TypeToken, const TArray<const TCHAR*>& Candidates);
	static void AddCompareTypeSuffixesForToken(const FString& TypeToken, TArray<FString>& Suffixes);
	static TArray<FString> BuildCompareTypeSuffixCandidates(const FBlueprintHelperGraphExpressionIR& Expression);
};
