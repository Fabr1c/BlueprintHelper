#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"

FString FBlueprintHelperGraphTokenUtils::NormalizeToken(const FString& Token)
{
	return Token.TrimStartAndEnd().ToLower();
}

FString FBlueprintHelperGraphTokenUtils::NormalizeOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

FString FBlueprintHelperGraphTokenUtils::NormalizeFieldToken(const FString& Token)
{
	return Token.TrimStartAndEnd().ToLower();
}

FString FBlueprintHelperGraphTokenUtils::NormalizeDelegateOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

FString FBlueprintHelperGraphTokenUtils::NormalizeScheduleOperationToken(const FString& ScheduleOperation)
{
	return ScheduleOperation.TrimStartAndEnd().ToLower();
}

FString FBlueprintHelperGraphTokenUtils::NormalizeSingletonControlQuery(const FString& Query)
{
	FString Normalized = Query.TrimStartAndEnd();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

FString FBlueprintHelperGraphTokenUtils::NormalizeOpOperationToken(const FString& Token)
{
	FString Clean = NormalizeFieldToken(Token);
	if (Clean.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperVersionCompat::RightChopInlineNoShrink(Clean, 3);
	}

	if (Clean == TEXT("+")) return TEXT("add");
	if (Clean == TEXT("-")) return TEXT("subtract");
	if (Clean == TEXT("*")) return TEXT("multiply");
	if (Clean == TEXT("/")) return TEXT("divide");
	if (Clean == TEXT(">")) return TEXT("greater");
	if (Clean == TEXT(">=")) return TEXT("greater_equal");
	if (Clean == TEXT("<")) return TEXT("less");
	if (Clean == TEXT("<=")) return TEXT("less_equal");
	if (Clean == TEXT("==") || Clean == TEXT("=")) return TEXT("equal");
	if (Clean == TEXT("!=") || Clean == TEXT("<>")) return TEXT("not_equal");
	if (Clean == TEXT("&&") || Clean == TEXT("and")) return TEXT("boolean_and");
	if (Clean == TEXT("||") || Clean == TEXT("or")) return TEXT("boolean_or");
	if (Clean == TEXT("!") || Clean == TEXT("not")) return TEXT("boolean_not");
	return Clean;
}

FString FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(const FString& First, const FString& Second)
{
	return First.TrimStartAndEnd().IsEmpty()
		? Second.TrimStartAndEnd()
		: First.TrimStartAndEnd();
}

FString FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(const FString& First, const FString& Second, const FString& Third)
{
	const FString Intermediate = First.TrimStartAndEnd().IsEmpty()
		? Second.TrimStartAndEnd()
		: First.TrimStartAndEnd();
	return Intermediate.TrimStartAndEnd().IsEmpty()
		? Third.TrimStartAndEnd()
		: Intermediate;
}

FString FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(const FString& First, const FString& Second, const FString& Third, const FString& Fourth)
{
	return FirstNonEmptyString(FirstNonEmptyString(First, Second, Third), Fourth);
}
