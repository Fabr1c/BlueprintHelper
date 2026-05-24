// BlueprintHelper GraphStatement expression/type utility helpers implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

void FBlueprintHelperGraphStatementTypeUtils::AddUniqueString(TArray<FString>& Values, const FString& Value)
{
	if (!Value.IsEmpty() && !Values.Contains(Value))
	{
		Values.Add(Value);
	}
}

FString FBlueprintHelperGraphStatementTypeUtils::SanitizeFragmentIdPart(const FString& Value)
{
	FString Clean = Value.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return TEXT("unnamed");
	}

	FString Result;
	Result.Reserve(Clean.Len());
	for (int32 Index = 0; Index < Clean.Len(); ++Index)
	{
		const TCHAR Character = Clean[Index];
		Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("unnamed") : Result;
}

FString FBlueprintHelperGraphStatementTypeUtils::ResolveExpressionKindName(
	const EBlueprintHelperGraphExpressionKind Kind)
{
	struct FExpressionKindRule
	{
		EBlueprintHelperGraphExpressionKind Kind;
		const TCHAR* Name;
	};

	static const FExpressionKindRule Rules[] =
	{
		{ EBlueprintHelperGraphExpressionKind::Literal, TEXT("literal") },
		{ EBlueprintHelperGraphExpressionKind::Field, TEXT("field") },
		{ EBlueprintHelperGraphExpressionKind::Call, TEXT("call") },
		{ EBlueprintHelperGraphExpressionKind::Op, TEXT("op") },
		{ EBlueprintHelperGraphExpressionKind::Construct, TEXT("construct") },
		{ EBlueprintHelperGraphExpressionKind::Deconstruct, TEXT("deconstruct") },
		{ EBlueprintHelperGraphExpressionKind::Select, TEXT("select") },
		{ EBlueprintHelperGraphExpressionKind::Create, TEXT("create") },
		{ EBlueprintHelperGraphExpressionKind::Convert, TEXT("convert") },
		{ EBlueprintHelperGraphExpressionKind::Schedule, TEXT("schedule") },
	};

	for (const FExpressionKindRule& Rule : Rules)
	{
		if (Rule.Kind == Kind)
		{
			return Rule.Name;
		}
	}
	return TEXT("unknown");
}

FString FBlueprintHelperGraphStatementTypeUtils::NormalizeOperatorToken(const FString& Operator)
{
	return Operator.TrimStartAndEnd().ToLower();
}

bool FBlueprintHelperGraphStatementTypeUtils::TokenMatches(
	const FString& Token,
	const TArray<const TCHAR*>& Candidates)
{
	for (const TCHAR* Candidate : Candidates)
	{
		if (Candidate && Token.Equals(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperGraphStatementTypeUtils::ResolveOperatorBaseName(const FString& Operator)
{
	struct FOperatorRule
	{
		const TCHAR* BaseName;
		TArray<const TCHAR*> Tokens;
	};

	const FString Token = NormalizeOperatorToken(Operator);
	static const FOperatorRule Rules[] =
	{
		{ TEXT("Add"), { TEXT("+"), TEXT("add"), TEXT("plus") } },
		{ TEXT("Subtract"), { TEXT("-"), TEXT("subtract"), TEXT("minus") } },
		{ TEXT("Multiply"), { TEXT("*"), TEXT("multiply"), TEXT("times") } },
		{ TEXT("Divide"), { TEXT("/"), TEXT("divide") } },
		{ TEXT("Greater"), { TEXT(">"), TEXT("gt"), TEXT("greater") } },
		{ TEXT("GreaterEqual"), { TEXT(">="), TEXT("gte"), TEXT("greater_equal"), TEXT("greaterequal") } },
		{ TEXT("Less"), { TEXT("<"), TEXT("lt"), TEXT("less") } },
		{ TEXT("LessEqual"), { TEXT("<="), TEXT("lte"), TEXT("less_equal"), TEXT("lessequal") } },
		{ TEXT("EqualEqual"), { TEXT("=="), TEXT("="), TEXT("eq"), TEXT("equal"), TEXT("equals") } },
		{ TEXT("NotEqual"), { TEXT("!="), TEXT("<>"), TEXT("ne"), TEXT("not_equal"), TEXT("notequal") } },
		{ TEXT("BooleanAND"), { TEXT("&&"), TEXT("and"), TEXT("boolean_and"), TEXT("booleanand") } },
		{ TEXT("BooleanOR"), { TEXT("||"), TEXT("or"), TEXT("boolean_or"), TEXT("booleanor") } },
	};

	for (const FOperatorRule& Rule : Rules)
	{
		if (TokenMatches(Token, Rule.Tokens))
		{
			return Rule.BaseName;
		}
	}
	return Operator.TrimStartAndEnd();
}

FString FBlueprintHelperGraphStatementTypeUtils::NormalizeOperatorTypeToken(const FString& Type)
{
	FString Token = Type;
	Token.TrimStartAndEndInline();
	Token.ToLowerInline();
	Token.ReplaceInline(TEXT(" "), TEXT(""));
	Token.ReplaceInline(TEXT("-"), TEXT(""));
	Token.ReplaceInline(TEXT("_"), TEXT(""));
	return Token;
}

bool FBlueprintHelperGraphStatementTypeUtils::TypeTokenMatches(
	const FString& TypeToken,
	const TArray<const TCHAR*>& Candidates)
{
	for (const TCHAR* Candidate : Candidates)
	{
		if (Candidate && TypeToken.Contains(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void FBlueprintHelperGraphStatementTypeUtils::AddOperatorTypeSuffixesForToken(
	const FString& TypeToken,
	TArray<FString>& Suffixes)
{
	struct FOperatorTypeRule
	{
		TArray<const TCHAR*> TypeTokens;
		TArray<const TCHAR*> Suffixes;
	};

	static const FOperatorTypeRule Rules[] =
	{
		{ { TEXT("bool") }, { TEXT("BoolBool") } },
		{ { TEXT("int64"), TEXT("long") }, { TEXT("Int64Int64") } },
		{ { TEXT("int"), TEXT("integer") }, { TEXT("IntInt") } },
		{ { TEXT("byte") }, { TEXT("ByteByte") } },
		{ { TEXT("double"), TEXT("real"), TEXT("number") }, { TEXT("DoubleDouble"), TEXT("FloatFloat") } },
		{ { TEXT("float") }, { TEXT("FloatFloat"), TEXT("DoubleDouble") } },
		{ { TEXT("string") }, { TEXT("StrStr") } },
		{ { TEXT("name") }, { TEXT("NameName") } },
		{ { TEXT("text") }, { TEXT("TextText") } },
		{ { TEXT("vector") }, { TEXT("VectorVector") } },
		{ { TEXT("rotator") }, { TEXT("RotatorRotator") } },
		{ { TEXT("transform") }, { TEXT("TransformTransform") } },
		{ { TEXT("object"), TEXT("actor"), TEXT("component") }, { TEXT("ObjectObject") } },
	};

	for (const FOperatorTypeRule& Rule : Rules)
	{
		if (TypeTokenMatches(TypeToken, Rule.TypeTokens))
		{
			for (const TCHAR* Suffix : Rule.Suffixes)
			{
				AddUniqueString(Suffixes, Suffix);
			}
		}
	}
}

TArray<FString> FBlueprintHelperGraphStatementTypeUtils::BuildOperatorTypeSuffixCandidates(
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	TArray<FString> Suffixes;
	if (Expression.Left.IsValid())
	{
		AddOperatorTypeSuffixesForToken(
			NormalizeOperatorTypeToken(Expression.Left->Type),
			Suffixes);
	}
	if (Expression.Right.IsValid())
	{
		AddOperatorTypeSuffixesForToken(
			NormalizeOperatorTypeToken(Expression.Right->Type),
			Suffixes);
	}
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
	{
		if (ArgPair.Value.IsValid())
		{
			AddOperatorTypeSuffixesForToken(
				NormalizeOperatorTypeToken(ArgPair.Value->Type),
				Suffixes);
		}
	}

	static const TCHAR* FallbackSuffixes[] =
	{
		TEXT("DoubleDouble"),
		TEXT("FloatFloat"),
		TEXT("IntInt"),
		TEXT("Int64Int64"),
		TEXT("BoolBool"),
		TEXT("ByteByte"),
		TEXT("ObjectObject"),
		TEXT("NameName"),
		TEXT("StrStr"),
		TEXT("TextText"),
		TEXT("VectorVector"),
		TEXT("RotatorRotator"),
		TEXT("TransformTransform"),
	};

	for (const TCHAR* Suffix : FallbackSuffixes)
	{
		AddUniqueString(Suffixes, Suffix);
	}
	return Suffixes;
}

FString FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString SourceId = !Expression.ExpressionId.IsEmpty() ? Expression.ExpressionId : Expression.Path;
	if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("["))
		&& !SourceId.Contains(TEXT("]")))
	{
		return SanitizeFragmentIdPart(SourceId);
	}

	const FString Suffix = ResolveExpressionKindName(Expression.Kind);
	return SanitizeFragmentIdPart(
		TEXT("expr_") + Suffix + TEXT("_") + SourceId + TEXT("_") + Suffix);
}

FString FBlueprintHelperGraphStatementTypeUtils::ResolveOperatorFunctionName(
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString RawOperator = Expression.Operator.TrimStartAndEnd();
	if (RawOperator.IsEmpty())
	{
		return FString();
	}

	const FString BaseName = ResolveOperatorBaseName(RawOperator);
	if (BaseName.IsEmpty())
	{
		return RawOperator;
	}

	return BaseName;
}
