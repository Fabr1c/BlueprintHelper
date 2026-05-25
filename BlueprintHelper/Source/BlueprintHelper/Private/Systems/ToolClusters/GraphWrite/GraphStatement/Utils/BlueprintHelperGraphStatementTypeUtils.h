// BlueprintHelper GraphStatement expression/type utility helpers.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

struct FBlueprintHelperGraphExpressionIR;

class FBlueprintHelperGraphStatementTypeUtils
{
public:
	static FString MakeExpressionFragmentId(const FBlueprintHelperGraphExpressionIR& Expression);
	static FString ResolveContainerActionResultTypeToken(
		const FString& ContainerKind,
		const FString& ContainerOperation,
		const FString& ElementType,
		const FString& KeyType,
		const FString& ValueType,
		const FString& PinType,
		const FString& KeyPinType,
		const FString& ValuePinType);
	static FString ResolveOperatorFunctionName(const FBlueprintHelperGraphExpressionIR& Expression);

private:
	static void AddUniqueString(TArray<FString>& Values, const FString& Value);
	static FString SanitizeFragmentIdPart(const FString& Value);
	static FString ResolveExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind);
	static FString NormalizeOperatorToken(const FString& Operator);
	static bool TokenMatches(const FString& Token, const TArray<const TCHAR*>& Candidates);
	static FString ResolveOperatorBaseName(const FString& Operator);
	static FString NormalizeOperatorTypeToken(const FString& Type);
	static bool TypeTokenMatches(const FString& TypeToken, const TArray<const TCHAR*>& Candidates);
	static void AddOperatorTypeSuffixesForToken(const FString& TypeToken, TArray<FString>& Suffixes);
	static TArray<FString> BuildOperatorTypeSuffixCandidates(const FBlueprintHelperGraphExpressionIR& Expression);
};
