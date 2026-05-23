#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

namespace
{
static FString GetStatementId(const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!Statement.StatementId.IsEmpty())
	{
		return Statement.StatementId;
	}
	if (!Statement.Path.IsEmpty())
	{
		return Statement.Path;
	}
	return TEXT("semantic_statement");
}

static FString GetStatementContextId(const FBlueprintHelperGraphStatementIR& Statement)
{
	return !Statement.StatementId.IsEmpty() ? Statement.StatementId : GetStatementId(Statement);
}

static void FillLiteralArgsAsDefaultsAndTypes(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
	TMap<FString, FString>& OutDefaultValues,
	TMap<FString, FString>& OutArgumentTypes)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Args)
	{
		if (!ArgPair.Value.IsValid())
		{
			continue;
		}
		if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
		}
		if (!ArgPair.Value->Type.TrimStartAndEnd().IsEmpty())
		{
			OutArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
		}
	}
}
}

bool FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions,
	const TMap<FString, FBlueprintHelperCallFunctionPinType>* SemanticArgumentPinTypes)
{
	const FString StatementId = GetStatementId(Statement);
	const FString StatementContextId = GetStatementContextId(Statement);
	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Call)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = !Statement.Target.IsEmpty() ? Statement.Target : Statement.Name;
		Request.ResolvedStableId = Statement.ResolvedCallFunctionStableId;
		FillLiteralArgsAsDefaultsAndTypes(Statement.Args, Request.DefaultValues, Request.ArgumentTypes);
		if (SemanticArgumentPinTypes)
		{
			Request.ArgumentPinTypes.Append(*SemanticArgumentPinTypes);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			OutCandidateFunctions,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& Statement.FieldOperation.Equals(TEXT("set"), ESearchCase::IgnoreCase)
		&& Statement.FieldScope.Equals(TEXT("variable"), ESearchCase::IgnoreCase))
	{
		const FString VariableName = !Statement.ResolvedTarget.Member.IsEmpty()
			? Statement.ResolvedTarget.Member
			: Statement.Target;
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Target = VariableName;
		if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			Request.DefaultValues.Add(VariableName, Statement.Value->LiteralValue);
			Request.DefaultValues.Add(TEXT("value"), Statement.Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& Statement.FieldOperation.Equals(TEXT("set"), ESearchCase::IgnoreCase)
		&& Statement.FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase))
	{
		const FString PropertyTarget = !Statement.ResolvedTarget.Member.IsEmpty()
			? Statement.ResolvedTarget.Member
			: (!Statement.Property.IsEmpty() ? Statement.Property : Statement.Target);
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Target = PropertyTarget;
		if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			Request.DefaultValues.Add(PropertyTarget, Statement.Value->LiteralValue);
			Request.DefaultValues.Add(TEXT("value"), Statement.Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildSetPropertyFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Branch
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Sequence
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Return)
	{
		return FBlueprintHelperControlFragmentBuilder::BuildStatement(
			TargetGraph,
			ActionContextScope,
			Statement,
			OutFragment,
			OutError);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::ComponentBoundEvent
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Delegate)
	{
		return FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(
			TargetGraph,
			ActionContextScope,
			Statement,
			OutFragment,
			OutError);
	}

	OutError = FString::Printf(TEXT("Semantic statement kind is not node-backed: %s."), *Statement.PatternName);
	return false;
}

bool FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildExpression(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	return FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
		TargetGraph,
		Expression,
		OutFragment,
		OutError,
		OutCandidateFunctions,
		ActionContextScope);
}
