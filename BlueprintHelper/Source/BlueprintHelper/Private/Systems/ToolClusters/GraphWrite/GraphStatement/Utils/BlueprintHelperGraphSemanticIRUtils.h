// BlueprintHelper GraphStatement FBlueprintHelperGraphSemanticIRUtils declarations.

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

class FBlueprintHelperGraphSemanticIRUtils
{
public:
	static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
	static FString JsonValueToSemanticType(const TSharedPtr<FJsonValue>& Value);
	static EBlueprintHelperGraphStatementKind ParseStatementKind(const FString& Kind);
	static EBlueprintHelperGraphExpressionKind ParseExpressionKind(const FString& Kind);
	static void AddDiagnostic(
			FBlueprintHelperGraphSemanticIR& OutIR,
			const FString& Code,
			const FString& Path,
			const FString& Message,
			const FString& Severity = TEXT("error"));
	static void ReadOptionalStringArrayField(
			const TSharedPtr<FJsonObject>& Object,
			const FString& FieldName,
			TArray<FString>& OutValues);
	static FString NormalizeSymbolKey(const FString& Name);
	static FString NormalizeSemanticTypeToken(const FString& Type);
	static bool IsSemanticBoolType(const FString& Type);
	static bool IsSemanticIntegerType(const FString& Type);
	static bool IsSemanticNumericType(const FString& Type);
	static bool AreSemanticTypesCompatible(const FString& Left, const FString& Right);
	static FString NormalizeTypeLookupKey(const FString& Name);
	static const UStruct* TryResolveStructByTypeName(const FString& TypeName);
	static FString StatementPatternName(EBlueprintHelperGraphStatementKind Kind);
	static FString ExpressionPatternName(EBlueprintHelperGraphExpressionKind Kind);
	static FBlueprintHelperGraphResolvedTarget ResolveTargetString(
			const FString& RawTarget,
			EBlueprintHelperGraphStatementKind StatementKind,
			EBlueprintHelperGraphExpressionKind ExpressionKind,
			const FBlueprintHelperGraphSemanticContext& Context);
	static void RegisterSymbol(
			FBlueprintHelperGraphSemanticIR& OutIR,
			const FString& Name,
			const FString& StatementId,
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& SourceExpression,
			const FString& Path,
			TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack);
	static void AddUnverifiedTargetDiagnostic(
			FBlueprintHelperGraphSemanticIR& OutIR,
			const FBlueprintHelperGraphSemanticContext& Context,
			const FBlueprintHelperGraphResolvedTarget& Target,
			const FString& Path);
	static bool FindSymbolInScopes(
			const FString& Name,
			const TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack,
			FBlueprintHelperGraphSymbol& OutSymbol);
};
